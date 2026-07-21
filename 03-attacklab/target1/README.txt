本文件包含此 attack lab 实例的相关材料。

文件说明：

    ctarget

具有代码注入（code-injection）漏洞的 Linux 二进制文件。用于完成作业的第 1-3 阶段。

    rtarget

具有面向返回编程（return-oriented programming）漏洞的 Linux 二进制文件。用于完成作业的第 4-5 阶段。

    cookie.txt

文本文件，包含本 lab 实例所需的 4 字节签名（signature）。

    farm.c

本 rtarget 实例中 gadget farm 的源代码。你可以通过编译（使用 -Og 标志）并反汇编来查找 gadgets。

    hex2raw

用于生成字节序列的实用程序。详见 lab 讲义中的文档。
