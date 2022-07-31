#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "meow_hash.h"

using namespace std;
/* meow_hash.h 已定义内容 */
#define INSTRUCTION_REORDER_BARRIER _ReadWriteBarrier()
#define prefetcht0(A) _mm_prefetch((char *)(A), _MM_HINT_T0)
#define movdqu(A, B)  A = _mm_loadu_si128((__m128i *)(B))
#define movdqu_mem(A, B)  _mm_storeu_si128((__m128i *)(A), B)
#define movq(A, B) A = _mm_set_epi64x(0, B);
#define aesdec(A, B)  A = _mm_aesdec_si128(A, B)
#define pshufb(A, B)  A = _mm_shuffle_epi8(A, B)
#define pxor(A, B)    A = _mm_xor_si128(A, B)
#define paddq(A, B) A = _mm_add_epi64(A, B)
#define pand(A, B)    A = _mm_and_si128(A, B)
#define palignr(A, B, i) A = _mm_alignr_epi8(A, B, i)
#define pxor_clear(A, B)    A = _mm_setzero_si128();

/* 补充解密以及相减函数 */
#define aesenc(A, B) A = _mm_aesenc_si128(A, B)
#define psubq(A, B)  A = _mm_sub_epi64(A, B)

/* MEOW_MIX_REG 的逆函数 */
#define MEOW_MIX_REG_inv(r1, r2, r3, r4, r5,  i1, i2, i3, i4) \
pxor(r4, i4);                \
psubq(r5, i3);               \
aesenc(r2, r4);              \
INSTRUCTION_REORDER_BARRIER; \
pxor(r2, i2);                \
psubq(r3, i1);               \
aesenc(r1, r2);              \
INSTRUCTION_REORDER_BARRIER;

/* MEOW_SHUFFLE 的逆函数 */
#define MEOW_SHUFFLE_inv(r1, r2, r3, r4, r5, r6) \
pxor(r2, r3);     \
aesenc(r4, r2);   \
psubq(r5, r6);    \
pxor(r4, r6);     \
psubq(r2, r5);    \
aesenc(r1, r4);

/* 仿照官方 PrintHash 函数编写打印函数 */
static void Printmeow128(meow_u128 meow1, meow_u128 meow2)
{
	printf("    %08X%08X%08X%08X%08X%08X%08X%08X\n",
		MeowU32From(meow1, 3),
		MeowU32From(meow1, 2),
		MeowU32From(meow1, 1),
		MeowU32From(meow1, 0),
		MeowU32From(meow2, 3),
		MeowU32From(meow2, 2),
		MeowU32From(meow2, 1),
		MeowU32From(meow2, 0));
}

/* 密钥求解(参考 meow_hash.h 中 MeowHash 函数) */
void KeyRecovery(meow_umm Len, void* Hashvalue, void* Msg)
{
	meow_u128 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
	// NOTE(casey): xmm0-xmm7 are the hash accumulation lanes
	meow_u128 xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15;
	// NOTE(casey): xmm8-xmm15 hold values to be appended (residual, length)

	meow_u8* rcx = (meow_u8*)Hashvalue;

	// NOTE(casey): Seed the eight hash registers
	
	movdqu(xmm0, rcx + 0x00);
	movdqu(xmm1, rcx + 0x10);
	movdqu(xmm2, rcx + 0x20);
	movdqu(xmm3, rcx + 0x30);

	movdqu(xmm4, rcx + 0x40);
	movdqu(xmm5, rcx + 0x50);
	movdqu(xmm6, rcx + 0x60);
	movdqu(xmm7, rcx + 0x70);

	/* 原 MeowHash 逆序 */
	/* squeeze */
	psubq(xmm0, xmm4);
	pxor(xmm4, xmm5);
	pxor(xmm0, xmm1);
	psubq(xmm5, xmm7);
	psubq(xmm4, xmm6);
	psubq(xmm1, xmm3);
	psubq(xmm0, xmm2);
	/* finalization */
	MEOW_SHUFFLE_inv(xmm3, xmm4, xmm5, xmm7, xmm0, xmm1);
	MEOW_SHUFFLE_inv(xmm2, xmm3, xmm4, xmm6, xmm7, xmm0);
	MEOW_SHUFFLE_inv(xmm1, xmm2, xmm3, xmm5, xmm6, xmm7);
	MEOW_SHUFFLE_inv(xmm0, xmm1, xmm2, xmm4, xmm5, xmm6);
	MEOW_SHUFFLE_inv(xmm7, xmm0, xmm1, xmm3, xmm4, xmm5);
	MEOW_SHUFFLE_inv(xmm6, xmm7, xmm0, xmm2, xmm3, xmm4);
	MEOW_SHUFFLE_inv(xmm5, xmm6, xmm7, xmm1, xmm2, xmm3);
	MEOW_SHUFFLE_inv(xmm4, xmm5, xmm6, xmm0, xmm1, xmm2);
	MEOW_SHUFFLE_inv(xmm3, xmm4, xmm5, xmm7, xmm0, xmm1);
	MEOW_SHUFFLE_inv(xmm2, xmm3, xmm4, xmm6, xmm7, xmm0);
	MEOW_SHUFFLE_inv(xmm1, xmm2, xmm3, xmm5, xmm6, xmm7);
	MEOW_SHUFFLE_inv(xmm0, xmm1, xmm2, xmm4, xmm5, xmm6);

	/* absorb message */
	// NOTE(casey): Load any less-than-32-byte residual
	pxor_clear(xmm9, xmm9);
	pxor_clear(xmm11, xmm11);

	// NOTE(casey): Load the part that is _not_ 16-byte aligned
	meow_u8* Last = (meow_u8*)Msg + (Len & ~0xf);
	int unsigned Len8 = (Len & 0xf);
	if (Len8)
	{
		// NOTE(casey): Load the mask early
		movdqu(xmm8, &MeowMaskLen[0x10 - Len8]);

		meow_u8* LastOk = (meow_u8*)((((meow_umm)(((meow_u8 *)Msg) + Len - 1)) | (MEOW_PAGESIZE - 1)) - 16);
		int Align = (Last > LastOk) ? ((int)(meow_umm)Last) & 0xf : 0;
		movdqu(xmm10, &MeowShiftAdjust[Align]);
		movdqu(xmm9, Last - Align);
		pshufb(xmm9, xmm10);

		// NOTE(jeffr): and off the extra bytes
		pand(xmm9, xmm8);
	}

	// NOTE(casey): We have to load the part that _is_ 16-byte aligned
	if (Len & 0x10)
	{
		xmm11 = xmm9;
		movdqu(xmm9, Last - 0x10);
	}

	// NOTE(casey): Construct the residual and length injests
	xmm8 = xmm9;
	xmm10 = xmm9;
	palignr(xmm8, xmm11, 15);
	palignr(xmm10, xmm11, 1);

	pxor_clear(xmm12, xmm12);
	pxor_clear(xmm13, xmm13);
	pxor_clear(xmm14, xmm14);
	movq(xmm15, Len);
	palignr(xmm12, xmm15, 15);
	palignr(xmm14, xmm15, 1);


	// Append the length, to avoid problems with our 32-byte padding
	MEOW_MIX_REG_inv(xmm1, xmm5, xmm7, xmm2, xmm3, xmm12, xmm13, xmm14, xmm15);

	// To maintain the mix-down pattern, we always Meow Mix the less-than-32-byte residual, even if it was empty
	MEOW_MIX_REG_inv(xmm0, xmm4, xmm6, xmm1, xmm2, xmm8, xmm9, xmm10, xmm11);

	Printmeow128(xmm0, xmm1);
	Printmeow128(xmm2, xmm3);
	Printmeow128(xmm4, xmm5);
	Printmeow128(xmm6, xmm7);

	return;
}



int main() {
	char Msg[21] = "Lina Li 202000210008";
	char HashValue[17] = "sdu_cst_20220610";

	cout << "原消息: " << Msg << endl << "哈希结果: " << HashValue << endl;
	cout << "得到密钥:" << endl;
	KeyRecovery(21, HashValue, Msg);
	cout << endl;

	system("pause");
	return 0;
}