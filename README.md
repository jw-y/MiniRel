# MiniRel

MiniRel is a simplified single-user relational database system for *Database System* course, Spring 2020, Seoul National University. It is implemented in C.

## Introduction

MiniRel is composed of five layers: the buffer pool (BF) layer, the paged file (PF) layer, the heap file (HF) layer, the access method (AM) layer, and the front-end (FE) layer. The BF layer is the lowest in the layered architencture of MiniRel.

<table>
    <tr align="center">
        <td colspan="2">DDL, DML, Query (<code>libfe.a</code>)</td>
    </tr>
    <tr align="center">
        <td>Heap File (<code>libhf.a</code>)</td>
        <td>Access Method (<code>libam.a</code>)</td>
    </tr>
    <tr align="center">
        <td colspan="2">Paged File (<code>libpf.a</code>)</td>
    </tr>
    <tr align="center">
        <td colspan="2">Buffer Manger (<code>libbf.a</code>)</td>
    </tr>
</table>

- Part I (the BF layer) and Part II (the PF layer) : the implementation of code to manage a buffer pool and to manipulate files that contain a sequence of pages, respectively.
- Part III (the HF layer) : the implementation of heap file management to provide the abstraction of random and sequential access to fixed-length records stored in files.
- Part IV (the AM layer) : the implementation of B+-tree index structure to speed up associative access to records.
- Part V (the FE layer) : the implementation of data definition language (DDL), data manipulation language (DML), and query language as well as catalog management

## How to Run

    make clean
    make

This will generate five function libraries: `libbf.a`, `libpf.a`, `libhf.a`, `libam.a` and `libfe.a`

To test each library,

    make clean
    make XXtest
    XXtest