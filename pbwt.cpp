#include "pbwt.h"

// temp
#include <iostream>
#include <bitset>
#include <cmath>//ceil

namespace djinn {

PBWT::PBWT() :
    n_symbols(0),
    n_samples(0),
    n_steps(0),
    prev(nullptr),
    ppa(nullptr),
    n_queue(nullptr),
    queue(nullptr),
    prev_bitmap(nullptr)
{

}

PBWT::PBWT(int64_t n_samples, int n_symbols) :
    n_symbols(n_symbols),
    n_samples(n_samples),
    n_steps(0),
    prev(new uint8_t[n_samples]),
    ppa(new uint32_t[n_samples]),
    n_queue(new uint32_t[n_symbols]),
    queue(new uint32_t*[n_symbols]),
    prev_bitmap(nullptr)
{
    assert(n_symbols > 1);

    for (int i = 0; i < n_symbols; ++i)
        queue[i] = new uint32_t[n_samples];

    memset(n_queue, 0, sizeof(uint32_t)*n_symbols);
    Reset();
}

PBWT::~PBWT() {
    delete[] prev;
    delete[] ppa;

    for (int i = 0; i < n_symbols; ++i)
        delete[] queue[i];

    delete[] queue;
    delete[] n_queue;
    delete[] prev_bitmap;
}

void PBWT::Initiate(int64_t n_s, int n_sym) {
    assert(n_sym > 1);

    n_samples = n_s;
    n_steps = 0;
    delete[] prev; delete[] ppa;
    delete[] n_queue;
    if (queue != nullptr) {
        for (int i = 0; i < n_symbols; ++i)
            delete[] queue[i];
    }
    n_symbols = n_sym;

    prev = new uint8_t[n_samples];
    ppa = new uint32_t[n_samples];
    n_queue = new uint32_t[n_symbols];
    queue = new uint32_t*[n_symbols];

    for (int i = 0; i < n_symbols; ++i)
        queue[i] = new uint32_t[n_samples];

    memset(n_queue, 0, sizeof(uint32_t)*n_symbols);
    Reset();
}

void PBWT::Reset() {
    if (ppa != nullptr) {
        for (int i = 0; i < n_samples; ++i)
            ppa[i] = i;
    }
    if (prev != nullptr) {
        memset(prev, 0, sizeof(uint8_t)*n_samples);
    }
    memset(n_queue, 0, sizeof(uint32_t)*n_symbols);
}

int PBWT::UpdateBcf(const uint8_t* arr, uint32_t stride) {
    // Reset queues.
    memset(n_queue, 0, sizeof(uint32_t)*n_symbols);

    for (int i = 0; i < n_samples; ++i) {
        const uint8_t& gt = BCF_UNPACK_GENOTYPE(arr[ppa[i] * stride]);
        // if (gt >= n_symbols) {
        //     std::cerr << "error=" << (int)arr[ppa[i]*stride] << "->" << (int)gt << ">=" << n_symbols << std::endl;
        // }
        assert(gt < n_symbols);
        queue[gt][n_queue[gt]++] = ppa[i];
        prev[i] = gt;
        // std::cerr << " " << (int)gt;
    }
    // std::cerr << std::endl << std::endl;

    uint32_t of = 0;
    for (int j = 0; j < n_symbols; ++j) {
        for (uint32_t i = 0; i < n_queue[j]; ++i, ++of)
            ppa[of] = queue[j][i];
    }
    //for (int i = 0; i < n_q2; ++i, ++of) ppa[of] = q2[i];
    //std::cerr << "of=" << of << "/" << n_samples << std::endl;
    assert(of == n_samples);
    // Debug: data is sorted at this point.
    // std::cerr << ToPrettyString() << std::endl;
    // std::cerr << "Alts=" << n_queue[1] << std::endl;
    // for (int i = 0; i < n_samples; ++i) {
    //   std::cerr << " " << (int)BCF_UNPACK_GENOTYPE(arr[ppa[i]*stride]);
    // }
    // std::cerr << std::endl;
    ++n_steps;

    return(1);
}

int PBWT::UpdateBcfGeneral(const uint8_t* arr, uint32_t stride) {
    // Reset queues.
    memset(n_queue, 0, sizeof(uint32_t)*n_symbols);

    for (int i = 0; i < n_samples; ++i) {
        const uint8_t& gt = BCF_UNPACK_GENOTYPE_GENERAL(arr[ppa[i] * stride]);
        if (gt >= n_symbols) {
            std::cerr << "error=" << (int)gt << "/" << n_symbols << std::endl;
        }
        assert(gt < n_symbols);
        for (int j = 0; j < n_symbols; ++j) {
            if (gt == j)
                queue[j][n_queue[j]++] = ppa[i];
        }
        prev[i] = gt;
    }

    uint32_t of = 0;
    for (int j = 0; j < n_symbols; ++j) {
        for (uint32_t i = 0; i < n_queue[j]; ++i, ++of)
            ppa[of] = queue[j][i];
    }
    assert(of == n_samples);
    ++n_steps;

    return(1);
}

int PBWT::Update(const uint8_t* arr, uint32_t stride) {
    // Reset queues.
    memset(n_queue, 0, sizeof(uint32_t)*n_symbols);

    for (int i = 0; i < n_samples; ++i) {
        const uint8_t& gt = arr[ppa[i] * stride];
        if (gt >= n_symbols) {
            std::cerr << "error=" << (int)gt << "/" << n_symbols << std::endl;
        }
        assert(gt < n_symbols);
        for (int j = 0; j < n_symbols; ++j) {
            if (gt == j)
                queue[j][n_queue[j]++] = ppa[i];
        }
        prev[i] = gt;
    }

    uint32_t of = 0;
    for (int j = 0; j < n_symbols; ++j) {
        for (uint32_t i = 0; i < n_queue[j]; ++i, ++of)
            ppa[of] = queue[j][i];
    }
    assert(of == n_samples);
    ++n_steps;

    return(1);
}

std::string PBWT::ToPrettyString() const {
    std::string ret = "n=" + std::to_string(n_samples) + " {";
    ret += std::to_string(ppa[0]);
    for (int i = 1; i < n_samples; ++i) {
        ret += "," + std::to_string(ppa[i]);
    }
    ret += "}";
    return(ret);
}

int PBWT::ReverseUpdate(const uint8_t* arr) {
    memset(prev, 0, n_samples); // O(n)
    memset(n_queue, 0, sizeof(uint32_t)*n_symbols);

    // Restore + update PPA
    // uint32_t n_skips = 0;
    for (int i = 0; i < n_samples; ++i) { // Worst case O(n), average case O(n) with a smallish constant.
        queue[arr[i]][n_queue[arr[i]]++] = ppa[i];
        if (arr[i] == 0) {
            // ++n_skips;
            continue;
        }
        prev[ppa[i]] = arr[i]; // Unpermute data.
    }

    // Merge PPA queues.
    uint32_t of = 0;
    for (int j = 0; j < n_symbols; ++j) { // O(n)
        memcpy(&ppa[of], queue[j], sizeof(uint32_t)*n_queue[j]);
        // for (int i = 0; i < n_queue[j]; ++i, ++of) {
        //     ppa[of] = queue[j][i];
        // }
        of += n_queue[j];
    }
    assert(of == n_samples);
    ++n_steps;

    // std::cerr << "nskips=" << n_skips << std::endl;

    return 1;
}

int PBWT::ReverseUpdateBitmap(const uint8_t* arr) {
    // memset(prev, 0, n_samples); // O(n)
    if (prev_bitmap == nullptr) prev_bitmap = new uint64_t[(int)ceil((float)n_samples/64)];
    memset(prev_bitmap, 0, (int)ceil((float)n_samples/64) * sizeof(uint64_t));
    memset(n_queue, 0, sizeof(uint32_t)*n_symbols);

    // Restore + update PPA
    // uint32_t n_skips = 0;
    for (int i = 0; i < n_samples; ++i) { // Worst case O(n), average case O(n) with a smallish constant.
        queue[arr[i]][n_queue[arr[i]]++] = ppa[i];
        prev_bitmap[ppa[i]/64] |= arr[i] << (ppa[i] % 64);
        // prev[ppa[i]] = arr[i]; // Unpermute data.
    }

    // Merge PPA queues.
    uint32_t of = 0;
    for (int j = 0; j < n_symbols; ++j) { // O(n)
        memcpy(&ppa[of], queue[j], sizeof(uint32_t)*n_queue[j]);
        // for (int i = 0; i < n_queue[j]; ++i, ++of) {
        //     ppa[of] = queue[j][i];
        // }
        of += n_queue[j];
    }
    assert(of == n_samples);
    ++n_steps;

    return 1;
}

}