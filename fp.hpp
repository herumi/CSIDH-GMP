#pragma once
#include <iostream>
#include <gmp.h>
#include <gmpxx.h>
#include <chrono>

using namespace std;

struct Point {
    Fp X, Z;	//inf is (0, 1)
};

struct Fp {
    int v;
    Fp(int _v=0) : v(_v) {}
    static const int N = 8; //64bit * 8 =512bit
    uint64_t buf[N]{};

    struct FpDbl {
        uint64_t buf[N * 2]{};
    };

    //montgomery剰余乗算
    static Fp inv4;
    static Fp sqrt4;    //4*(sqrt(mod))
    static size_t nbit;
    static Fp R;
    static Fp R2;
    static Fp nr;

    static Fp mrR2;
    static Fp MR2;  //2* para.R2
    static Fp MR4;  //4* para.R2


    //足し算 Mod p OK
    static void add(Fp& z, const Fp& x, const Fp& y)
    {
        mpn_add_n(z.buf, x.buf, y.buf, N);
        if (mpn_cmp(z.buf, p.buf, N) >= 0)
            mpn_sub_n(z.buf, z.buf, p.buf, N);
    }

    //引き算 Mod p OK
    static void sub(Fp& z, const Fp& x, const Fp& y)
    {
        if (mpn_cmp(x.buf, y.buf, N) >= 0) {
            mpn_sub_n(z.buf, x.buf, y.buf, N);
        } else {
            mpn_add_n(z.buf, x.buf, p.buf, N);
            mpn_sub_n(z.buf, z.buf, y.buf, N);
        }
    }

    //MR    In:1024bit, Out:512bit
    static void MR(Fp& z, const FpDbl& x)
    {
        uint64_t temp[N * 2]{}; //初期化しろ！
        uint64_t temp512[N]{};

        mpn_and_n(temp512, x.buf, p.R.buf, N * 2);
        mpn_mul_n(temp, temp512, p.nr.buf, N);
        mpn_and_n(temp512, temp, p.R.buf, N * 2);
        mpn_mul_n(temp, temp512, p.p.buf, N);
        mpn_add_n(temp, temp, x.buf, N * 2);
        mpn_rshift(temp, temp, N*2, p.nbit);

        for (int i = 0; i < 8; i++) {
            temp512[i] = temp[i+7];
        }

        if (mpn_cmp(temp512, p.p.buf, N) >= 0)
            mpn_sub_n(z.buf, temp512, p.p.buf, N);
        else {
            for (int i = 0; i < N; i++) {
                z.buf[i] = temp512[i];
            }
        }
    }

    //MR    In:512bit, Out:512bit
    static void MR512(Fp& z, const Fp& x)
    {
        uint64_t temp[N * 2]{}; //初期化しろ！
        uint64_t temp512[N]{};

        mpn_and_n(temp512, x.buf, p.R.buf, N);
        mpn_mul_n(temp, temp512, p.nr.buf, N);
        mpn_and_n(temp512, temp, p.R.buf, N * 2);
        mpn_mul_n(temp, temp512, p.p.buf, N);
        mpn_add_n(temp, temp, x.buf, N * 2);
        mpn_rshift(temp, temp, N * 2, p.nbit);

        for (int i = 0; i < 8; i++) {
            temp512[i] = temp[i + 7];
        }

        if (mpn_cmp(temp512, p.p.buf, N) >= 0)
            mpn_sub_n(z.buf, temp512, p.p.buf, N);
        else {
            for (int i = 0; i < N; i++) {
                z.buf[i] = temp512[i];
            }
        }
    }

    //掛け算 Mod p(1回で完結)
    static void mul_test(Fp& z, const Fp& x, const Fp& y)
    {
        mpz_class test;
        Fp temp512{};
        FpDbl temp{};
        mpn_mul_n(temp.buf, x.buf, y.buf,N);
        MR(temp512,temp);
        mpn_mul_n(temp.buf, temp512.buf, p.R2.buf, N);
        MR(z, temp);
    }

    //掛け算 Mod p(Montgomeryで返す)
    static void mul(Fp& z, const Fp& x, const Fp& y)
    {
        Fp temp512{};
        FpDbl temp{};
        mpn_mul_n(temp.buf, x.buf, y.buf, N);
        MR(z, temp);
    }

    static void sqr(Fp& z, const Fp& x)
    {
        Fp temp512{};
        FpDbl temp{};
        mpn_sqr(temp.buf, x.buf, N);
        MR(z, temp);
    }

    static void pow(Fp& z, const Fp& x, const Fp& y)
    {
        Fp result;
        FpDbl R2_temp{}, result_temp;

        for (int i = 0; i < N; i++) {
            R2_temp.buf[i] = p.R2.buf[i];
        }
        MR(result, R2_temp);

        mpz_class y_mpz;
        mpz_import(y_mpz.get_mpz_t(), Fp::N, -1, 8, 0, 0, y.buf);
        for (int i = nbit - 1; i >= 0; i--) {
            mpn_sqr(result_temp.buf, result.buf, N);
            MR(result, result_temp);
            if (mpz_tstbit(y_mpz.get_mpz_t(), i) == 1) {
                mpn_mul_n(result_temp.buf, result.buf, x.buf, N);
                MR(result, result_temp);
            }
        }
        z = result;
    }

    static void inv(Fp& z, const Fp& y)
    {
        if (mpn_zero_p(y.buf, N))
            throw std::range_error("Divided by zero.");
        else {
            Fp modhiku2;
            uint64_t ni[N]{};
            ni[0] = 2;
            mpn_sub_n(modhiku2.buf, p.p.buf, ni, N);
            pow(z, y, modhiku2);
        }

    }

    static void div(Fp& z, const Fp& x, const Fp& y)
    {
        FpDbl x_temp{}, y_temp{},temp1024;
        Fp temp,xmon,ymon;

        mpn_mul_n(x_temp.buf, x.buf, p.R2.buf, N);
        mpn_mul_n(y_temp.buf, y.buf, p.R2.buf, N);

        MR(xmon, x_temp);
        MR(ymon, y_temp);
        inv(temp, ymon);
        mul(xmon, xmon, temp);
        for (int i = 0; i < N; i++) {
            temp1024.buf[i] = xmon.buf[i];
        }
        MR(z, temp1024);
    }

    static Fp p;

};