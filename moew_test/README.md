# Meow Hash 密钥恢复

此项目仅做出尝试，并未进行结果验证。

## 实验原理
    
参考课上内容以及 https://peter.website/meow-hash-cryptanalysis 中相关内容，尝试直接对 meow_hash_x64_aesni.h 中 MeowHash 函数编写对应逆函数。

## 文件说明
meow_hash.h 将 meow_hash_x64_aesni.h 重命名为 meow_hash.h(c++)

KeyRecovery.cpp 实现密钥恢复(c++)

其他文件 生成可执行文件需要的文件

## 代码分析
原 hash 过程为：1）Absorb Message、2）Finalization、3）Squeeze

逆序为：1）Squeeze、2）Finalization、3）Absorb Message

查看 meow_hash.h 中 MeowHash 函数，发现以下内容完成 Squeeze 部分

    paddq(xmm0, xmm2);
    paddq(xmm1, xmm3);
    paddq(xmm4, xmm6);
    paddq(xmm5, xmm7);
    pxor(xmm0, xmm1);
    pxor(xmm4, xmm5);
    paddq(xmm0, xmm4);

对其实现逆序，需要定义 psubq(A, B) 完成相减，相关代码如下：

    #define psubq(A, B)  A = _mm_sub_epi64(A, B)

Finalization 部分由 12 轮 MEOW_SHUFFLE 完成，因此逆序需要定义对应逆函数 MEOW_SHUFFLE_inv ；定义逆函数时，aesdec 的逆对应 aesenc，相关代码如下：

    #define aesenc(A, B) A = _mm_aesenc_si128(A, B)
    #define MEOW_SHUFFLE_inv(r1, r2, r3, r4, r5, r6) \
    pxor(r2, r3);     \
    aesenc(r4, r2);   \
    psubq(r5, r6);    \
    pxor(r4, r6);     \
    psubq(r2, r5);    \
    aesenc(r1, r4);

Absorb Message 部分由多个部分完成，此处并未完全理解，只大概调整顺序，将 MEOW_MIX_REG 调整至最后，并定义其逆函数 MEOW_MIX_REG_inv ，相关代码如下：

    #define MEOW_MIX_REG_inv(r1, r2, r3, r4, r5,  i1, i2, i3, i4) \
    pxor(r4, i4);                \
    psubq(r5, i3);               \
    aesenc(r2, r4);              \
    INSTRUCTION_REORDER_BARRIER; \
    pxor(r2, i2);                \
    psubq(r3, i1);               \
    aesenc(r1, r2);              \
    INSTRUCTION_REORDER_BARRIER;

最后在 main 函数实现字符串定义即可。

由于信息较短，且对 Meow Hash 理解并不充足，因此并未实现结果的验证。

## 运行指导

变量定义完成，visual studio打开.sln项目文件后执行.cpp文件即可

## 密钥恢复结果展示

![图片未加载](https://github.com/l921n/chaos/blob/main/02.png "MeowHash 密钥恢复结果展示")
