#include <cstring> //memcpy

// #include "ctx_model.h"
#include "djinn.h"
#include "frequency_model.h" // RangeCoder and FrequencyModel
#include "pbwt.h" // PBWT algorithms

namespace djinn {

/*======   Context container   ======*/

djn_ctx_model_t::djn_ctx_model_t() :
    p(nullptr),
    p_len(0), p_cap(0), p_free(false),
    n_variants(0)
{
    
}

djn_ctx_model_t::~djn_ctx_model_t() {
    if (p_free) delete[] p;
}

void djn_ctx_model_t::Initiate2mc() {
    pbwt       = std::make_shared<PBWT>();
    range_coder= std::make_shared<RangeCoder>();
    mref       = std::make_shared<GeneralModel>(2,   512, 18, 32, range_coder);
    mlog_rle   = std::make_shared<GeneralModel>(16,  32,  18, 16, range_coder); // 2^32 max -> log2() = 5
    mrle       = std::make_shared<GeneralModel>(256, 64,  18, 32, range_coder); // 1 bit ref, 5 bit alt
    mrle2_1    = std::make_shared<GeneralModel>(256, 64,  18, 8,  range_coder);
    mrle2_2    = std::make_shared<GeneralModel>(256, 64,  18, 8,  range_coder);
    mrle4_1    = std::make_shared<GeneralModel>(256, 64,  18, 8,  range_coder);
    mrle4_2    = std::make_shared<GeneralModel>(256, 64,  18, 8,  range_coder);
    mrle4_3    = std::make_shared<GeneralModel>(256, 64,  18, 8,  range_coder);
    mrle4_4    = std::make_shared<GeneralModel>(256, 64,  18, 8,  range_coder);
    dirty_wah  = std::make_shared<GeneralModel>(256, 256, 18, 16, range_coder);
    mtype      = std::make_shared<GeneralModel>(2,   512, 18, 1,  range_coder);
}

void djn_ctx_model_t::InitiateNm() {
    pbwt       = std::make_shared<PBWT>();
    range_coder= std::make_shared<RangeCoder>();
    mref       = std::make_shared<GeneralModel>(16,  4096,18, 32, range_coder);
    mlog_rle   = std::make_shared<GeneralModel>(64,  32,  18, 16, range_coder); // 4 bit ref, log2(32) = 5 bit -> 2^9 = 512
    mrle       = std::make_shared<GeneralModel>(256, 512, 24, 1,  range_coder); // 4 bits ref + 5 bits log2(run) -> 2^9
    mrle2_1    = std::make_shared<GeneralModel>(256, 512, range_coder);
    mrle2_2    = std::make_shared<GeneralModel>(256, 512, range_coder);
    mrle4_1    = std::make_shared<GeneralModel>(256, 512, range_coder);
    mrle4_2    = std::make_shared<GeneralModel>(256, 512, range_coder);
    mrle4_3    = std::make_shared<GeneralModel>(256, 512, range_coder);
    mrle4_4    = std::make_shared<GeneralModel>(256, 512, range_coder);
    dirty_wah  = std::make_shared<GeneralModel>(256, 256, 18, 32, range_coder);
    mtype      = std::make_shared<GeneralModel>(2,   512, 18, 1,  range_coder);
}

void djn_ctx_model_t::reset() {
    // std::cerr << "[djn_ctx_model_t::reset] resetting" << std::endl;
    // range_coder = std::make_shared<RangeCoder>();
    if (mref.get() != nullptr)      mref->Reset();
    if (mlog_rle.get() != nullptr)  mlog_rle->Reset();
    if (mrle.get() != nullptr)      mrle->Reset();
    if (mrle2_1.get() != nullptr)   mrle2_1->Reset(); 
    if (mrle2_2.get() != nullptr)   mrle2_2->Reset();
    if (mrle4_1.get() != nullptr)   mrle4_1->Reset(); 
    if (mrle4_2.get() != nullptr)   mrle4_2->Reset(); 
    if (mrle4_3.get() != nullptr)   mrle4_3->Reset(); 
    if (mrle4_4.get() != nullptr)   mrle4_4->Reset();
    if (dirty_wah.get() != nullptr) dirty_wah->Reset();
    if (mtype.get() != nullptr)     mtype->Reset();
    if (mref.get() != nullptr)      mref->Reset();
    if (pbwt.get() != nullptr)      pbwt->Reset();
    n_variants = 0; p_len = 0;
}

int djn_ctx_model_t::StartEncoding(bool use_pbwt, bool reset) {
    if (range_coder.get() == nullptr) return -1;

    if (p == nullptr) { // initiate a buffer if there is none
        delete[] p;
        p_cap = 10000000;
        p = new uint8_t[p_cap];
        p_len = 0;
        p_free = true;
    }
    if (reset) this->reset();
    p_len = 0;

    range_coder->SetOutput(p);
    range_coder->StartEncode();
    return 1;
}

size_t djn_ctx_model_t::FinishEncoding() {
    if (range_coder.get() == nullptr) return -1;
    range_coder->FinishEncode();
    p_len = range_coder->OutSize();
    return range_coder->OutSize();
}

int djn_ctx_model_t::StartDecoding(bool use_pbwt, bool reset) {
    if (range_coder.get() == nullptr) return -1;
    if (reset) this->reset();
    if (p == nullptr) return -2; // or result in corruption as range coder immediately loads data

    range_coder->SetInput(p);
    range_coder->StartDecode();
    return 1;
}

int djn_ctx_model_t::Serialize(uint8_t* dst) const {
    // Serialize as (uint32_t,uint32_t,uint8_t*):
    // p_len, n_variants, p
    uint32_t offset = 0;
    *((uint32_t*)&dst[offset]) = p_len; // data length
    offset += sizeof(uint32_t);
    *((uint32_t*)&dst[offset]) = n_variants; // number of variants
    offset += sizeof(uint32_t);
    memcpy(&dst[offset], p, p_len); // data
    offset += p_len;

    // assert(range_coder->OutSize() == p_len);
    return offset;
}

int djn_ctx_model_t::Serialize(std::ostream& stream) const {
    // Serialize as (uint32_t,uint32_t,uint8_t*):
    // p_len, n_variants, p
    stream.write((char*)&p_len, sizeof(uint32_t));
    stream.write((char*)&n_variants, sizeof(uint32_t));
    stream.write((char*)p, p_len);
    return stream.tellp();
}

int djn_ctx_model_t::GetSerializedSize() const {
    int ret = sizeof(uint32_t) + sizeof(uint32_t) + p_len;
    return ret;
}

int djn_ctx_model_t::GetCurrentSize() const {
    if (range_coder.get() == nullptr) return -1;
    return range_coder->OutSize();
}

int djn_ctx_model_t::Deserialize(uint8_t* dst) {
    uint32_t offset = 0;
    p_len = *((uint32_t*)&dst[offset]);
    offset += sizeof(uint32_t);
    n_variants = *((uint32_t*)&dst[offset]);
    offset += sizeof(uint32_t);

    // initiate a buffer if there is none or it's too small
    if (p_cap == 0 || p == nullptr || p_len > p_cap) {
        // std::cerr << "[Deserialize] Limit. p_cap=" << p_cap << "," << "p is nullptr=" << (p == nullptr ? "yes" : "no") << ",p_len=" << p_len << "/" << p_cap << std::endl;
        if (p_free) delete[] p;
        p = new uint8_t[p_len + 65536];
        p_cap = p_len + 65536;
        p_free = true;
    }

    memcpy(p, &dst[offset], p_len); // data
    offset += p_len;
    
    return offset;
}

int djn_ctx_model_t::Deserialize(std::istream& stream) {
    // Serialize as (uint32_t,uint32_t,uint8_t*):
    // p_len, n_variants, p
    stream.read((char*)&p_len, sizeof(uint32_t));
    stream.read((char*)&n_variants, sizeof(uint32_t));

    // initiate a buffer if there is none or it's too small
    if (p_cap == 0 || p == nullptr || p_len > p_cap) {
        // std::cerr << "[Deserialize] Limit. p_cap=" << p_cap << "," << "p is nullptr=" << (p == nullptr ? "yes" : "no") << ",p_len=" << p_len << "/" << p_cap << std::endl;
        if (p_free) delete[] p;
        p = new uint8_t[p_len + 65536];
        p_cap = p_len + 65536;
        p_free = true;
    }

    stream.read((char*)p, p_len);
    return stream.tellg();
}

/*======   Variant context model   ======*/

djinn_ctx_model::djinn_ctx_model() : 
    p(new uint8_t[1000000]), p_len(0), p_cap(1000000), p_free(true),
    q(nullptr), q_len(0), q_alloc(0), q_free(true),
    range_coder(std::make_shared<RangeCoder>()), 
    ploidy_dict(std::make_shared<GeneralModel>(256, 256, range_coder))
{
    
}

djinn_ctx_model::~djinn_ctx_model() { 
    if (p_free) delete[] p;
    if (q_free) delete[] q;
}

int djinn_ctx_model::EncodeBcf(uint8_t* data, size_t len_data, int ploidy, uint8_t alt_alleles) {
    if (data == nullptr) return -2;
    if (len_data % ploidy != 0) return -3;
    // Currently limited to 14 alt alleles + missing + EOV marker (total of 16).
    assert(alt_alleles < 14);

    // Todo: check that ploidy_dict model is initiated!
    std::shared_ptr<djn_ctx_model_container_t> tgt_container;

    const uint64_t tuple = ((uint64_t)len_data << 32) | ploidy;
    auto search = ploidy_map.find(tuple);
    if (search != ploidy_map.end()) {
        tgt_container = ploidy_models[search->second];
        ploidy_dict->EncodeSymbol(search->second);
    } else {
        // std::cerr << "Not found. Inserting: " << len_data << "," << ploidy << "(" << tuple << ") as " << ploidy_models.size() << std::endl;
        ploidy_map[tuple] = ploidy_models.size();
        ploidy_dict->EncodeSymbol(ploidy_models.size());
        ploidy_models.push_back(std::make_shared<djn_ctx_model_container_t>(len_data, ploidy, (bool)use_pbwt));
        tgt_container = ploidy_models[ploidy_models.size() - 1];
        tgt_container->StartEncoding(use_pbwt, init);
    }
    assert(tgt_container.get() != nullptr);

    // Compute allele counts.
    memset(hist_alts, 0, 256*sizeof(uint32_t));
    for (int i = 0; i < len_data; ++i) {
        ++hist_alts[DJN_BCF_UNPACK_GENOTYPE_GENERAL(data[i])];
    }
    // Check.
    assert(tgt_container->marchetype.get() != nullptr);

    // for (int i = 0; i < 256; ++i) {
    //     if (hist_alts[i]) std::cerr << i << ":" << hist_alts[i] << ",";
    // }
    // std::cerr << " n_variants=" << tgt_container->n_variants << " data2m=" << tgt_container->model_2mc->range_coder->OutSize() << " dataNm=" << tgt_container->model_nm->range_coder->OutSize() << " permute=" << (int)use_pbwt << std::endl;

    // Biallelic, no missing, and no special EOV symbols.
    if (alt_alleles <= 2 && hist_alts[14] == 0 && hist_alts[15] == 0) {
        tgt_container->marchetype->EncodeSymbol(0); // add archtype as 2mc

        int ret = -1;
        if (use_pbwt) {
            // Todo: add lower limit to stored parameters during serialization
            if (hist_alts[1] < 10) { // dont update if < 10 alts
                for (int i = 0; i < len_data; ++i) {
                    tgt_container->model_2mc->pbwt->prev[i] = DJN_BCF_UNPACK_GENOTYPE(data[tgt_container->model_2mc->pbwt->ppa[i]]);
                }
            } else {
                tgt_container->model_2mc->pbwt->UpdateBcf(data, 1);
            }

            ret = (tgt_container->Encode2mc(tgt_container->model_2mc->pbwt->prev, len_data));
        } else {
            ret = (tgt_container->Encode2mc(data, len_data, DJN_BCF_GT_UNPACK, 1));
        }

        if (ret > 0) ++n_variants;
        return ret;
    } else { // Otherwise.
        tgt_container->marchetype->EncodeSymbol(1);
        
        int ret = -1;
        if (use_pbwt) {
            tgt_container->model_nm->pbwt->UpdateBcfGeneral(data, 1); // otherwise
            ret = (tgt_container->EncodeNm(tgt_container->model_nm->pbwt->prev, len_data));
        } else {
            ret = (tgt_container->EncodeNm(data, len_data, DJN_BCF_GT_UNPACK_GENERAL, 1));
        }
        if (ret > 0) ++n_variants;
        return ret;
    }
}

int djinn_ctx_model::Encode(uint8_t* data, size_t len_data, int ploidy, uint8_t alt_alleles) {
    if (data == nullptr) return -2;
    if (len_data % ploidy != 0) return -3;
    // Currently limited to 14 alt alleles + missing + EOV marker (total of 16).
    assert(alt_alleles < 14);

    // Todo: check that ploidy_dict model is initiated!
    std::shared_ptr<djn_ctx_model_container_t> tgt_container;

    const uint64_t tuple = ((uint64_t)len_data << 32) | ploidy;
    auto search = ploidy_map.find(tuple);
    if (search != ploidy_map.end()) {
        tgt_container = ploidy_models[search->second];
        ploidy_dict->EncodeSymbol(search->second);
    } else {
        // std::cerr << "Not found. Inserting: " << len_data << "," << ploidy << "(" << tuple << ") as " << ploidy_models.size() << std::endl;
        ploidy_map[tuple] = ploidy_models.size();
        ploidy_dict->EncodeSymbol(ploidy_models.size());
        ploidy_models.push_back(std::make_shared<djn_ctx_model_container_t>(len_data, ploidy, (bool)use_pbwt));
        tgt_container = ploidy_models[ploidy_models.size() - 1];
        tgt_container->StartEncoding(use_pbwt, init); // Todo: fix me
    }
    assert(tgt_container.get() != nullptr);

    // Compute allele counts.
    memset(hist_alts, 0, 256*sizeof(uint32_t));
    for (int i = 0; i < len_data; ++i) {
        ++hist_alts[data[i]];
    }
    // Check.
    assert(tgt_container->marchetype.get() != nullptr);

    // for (int i = 0; i < 256; ++i) {
    //     if (hist_alts[i]) std::cerr << i << ":" << hist_alts[i] << ",";
    // }
    // std::cerr << " data2m=" << tgt_container->model_2mc->range_coder->OutSize() << " dataNm=" << tgt_container->model_nm->range_coder->OutSize() << " permute=" << (bool)use_pbwt << std::endl;

    // Biallelic, no missing, and no special EOV symbols.
    if (alt_alleles <= 2 && hist_alts[14] == 0 && hist_alts[15] == 0) {
        tgt_container->marchetype->EncodeSymbol(0); // add archtype as 2mc

        int ret = -1;
        if (use_pbwt) {
            // Todo: add lower limit to stored parameters during serialization
            if (hist_alts[1] < 10) { // dont update if < 10 alts
                for (int i = 0; i < len_data; ++i) {
                    tgt_container->model_2mc->pbwt->prev[i] = data[tgt_container->model_2mc->pbwt->ppa[i]];
                }
            } else {
                tgt_container->model_2mc->pbwt->Update(data, 1);
            }

            ret = (tgt_container->Encode2mc(tgt_container->model_2mc->pbwt->prev, len_data));
        } else {
            ret = (tgt_container->Encode2mc(data, len_data, DJN_MAP_NONE, 0));
        }

        if (ret > 0) ++n_variants;
        return ret;
    } else { // Otherwise.
        tgt_container->marchetype->EncodeSymbol(1);
        
        int ret = -1;
        if (use_pbwt) {
            tgt_container->model_nm->pbwt->Update(data, 1);
            ret = (tgt_container->EncodeNm(tgt_container->model_nm->pbwt->prev, len_data));
        } else {
            ret = (tgt_container->EncodeNm(data, len_data, DJN_MAP_NONE, 0));
        }
        if (ret > 0) ++n_variants;
        return ret;
    }
}

int djinn_ctx_model::DecodeNext(uint8_t* ewah_data, uint32_t& ret_ewah, uint8_t* ret_buffer, uint32_t& ret_len) {
    if (ewah_data == nullptr) return -1;
    if (ret_buffer == nullptr) return -1;

    // Decode stream archetype.
    uint8_t type = ploidy_dict->DecodeSymbol();
    std::shared_ptr<djn_ctx_model_container_t> tgt_container = ploidy_models[type];

    return(tgt_container->DecodeNext(ewah_data,ret_ewah,ret_buffer,ret_len));
}

int djinn_ctx_model::DecodeNext(djinn_variant_t*& variant) {
    // Decode stream archetype.
    uint8_t type = ploidy_dict->DecodeSymbol();
    std::shared_ptr<djn_ctx_model_container_t> tgt_container = ploidy_models[type];

    if (variant == nullptr) {
        variant = new djinn_variant_t;
        variant->data_alloc = tgt_container->n_samples + 65536;
        variant->data = new uint8_t[variant->data_alloc];
        variant->data_free = true;
    } else if (tgt_container->n_samples >= variant->data_alloc) {
        if (variant->data_free) delete[] variant->data;
        variant->data_alloc = tgt_container->n_samples + 65536;
        variant->data = new uint8_t[variant->data_alloc];
        variant->data_free = true;
    }

    if (q == nullptr) {
        q_alloc = tgt_container->n_samples + 65536;
        q = new uint8_t[q_alloc];
        q_len = 0;
        q_free = true;
    } else if (tgt_container->n_samples >= q_alloc) {
        if (q_free) delete[] q;
        q_alloc = tgt_container->n_samples + 65536;
        q = new uint8_t[q_alloc];
        q_len = 0;
        q_free = true;
    }
    q_len = 0; // Reset q_len for next iteration.

    variant->ploidy   = tgt_container->ploidy;
    variant->data_len = 0;
    variant->errcode  = 0;
    variant->unpacked = DJN_UN_IND;
    variant->n_allele = 0;

    int ret = tgt_container->DecodeNext(q,q_len,variant->data,variant->data_len);
    if (ret <= 0) {
        variant->errcode = 1;
        return ret;
    }

    // Compute number of alleles.
    uint32_t max_allele = 0;
    for (int i = 0; i < 256; ++i) {
        max_allele = tgt_container->hist_alts[i] != 0 ? i : max_allele;
        // if (tgt_container->hist_alts[i]) std::cerr << i << ":" << tgt_container->hist_alts[i] << ",";
    }
    // std::cerr << " max=" << max_allele << std::endl;
    variant->n_allele = max_allele + 1;

    return 1;
}

int djinn_ctx_model::DecodeNextRaw(uint8_t* data, uint32_t& len) {
    if (data == nullptr) return -1;
    uint8_t type = ploidy_dict->DecodeSymbol();

    std::shared_ptr<djn_ctx_model_container_t> tgt_container = ploidy_models[type];
    return(tgt_container->DecodeNextRaw(data, len));
}

int djinn_ctx_model::DecodeNextRaw(djinn_variant_t*& variant) {
    // Decode stream archetype.
    uint8_t type = ploidy_dict->DecodeSymbol();
    std::shared_ptr<djn_ctx_model_container_t> tgt_container = ploidy_models[type];
    return tgt_container->DecodeNextRaw(variant);
}

void djinn_ctx_model::StartEncoding(bool use_pbwt, bool reset) {
    // Store parameters for decoding.
    this->use_pbwt = use_pbwt;
    this->init = reset;

    n_variants = 0;
    p_len = 0;

    // Local range coder
    range_coder->SetOutput(p);
    range_coder->StartEncode();

    for (int i = 0; i < ploidy_models.size(); ++i) {
        ploidy_models[i]->StartEncoding(use_pbwt, reset);
    }
}

size_t djinn_ctx_model::FinishEncoding() {
    if (range_coder.get() == nullptr) return -1;
    range_coder->FinishEncode();
    p_len = range_coder->OutSize();

    size_t s_models = 0;
    for (int i = 0; i < ploidy_models.size(); ++i) {
        s_models += ploidy_models[i]->FinishEncoding();
    }
    size_t s_rc  = range_coder->OutSize();
    
    return s_rc + s_models;
}

int djinn_ctx_model::StartDecoding() {
    assert(p != nullptr);
    assert(ploidy_models.size() != 0);

    for (int i = 0; i <ploidy_models.size(); ++i) {
        ploidy_models[i]->StartDecoding(use_pbwt, init);
    }

    range_coder->SetInput(p);
    range_coder->StartDecode();
    return 1;
}

// Read/write
int djinn_ctx_model::Serialize(uint8_t* dst) const {
    // Serialize as (int,uint32_t,uint32_t,uint8_t*,ctx1,ctx2):
    // #models,p_len,p,[models...]
    uint32_t offset = 0;

    // Reserve space for offset
    offset += sizeof(uint32_t);

    // Add
    *((int*)&dst[offset]) = (int)ploidy_models.size(); // number of models
    offset += sizeof(int);
    *((uint32_t*)&dst[offset]) = n_variants; // number of variants
    offset += sizeof(uint32_t);

    // Serialize bit-packed controller.
    uint8_t pack = (use_pbwt << 7) | (init << 6) | (unused << 0);
    dst[offset] = pack;
    offset += sizeof(uint8_t);

    *((uint32_t*)&dst[offset]) = p_len; // data length
    offset += sizeof(uint32_t);
    
    // Store model selection data.
    memcpy(&dst[offset], p, p_len); // data
    offset += p_len;

    // Serialize each model.
    for (int i = 0; i < ploidy_models.size(); ++i) {
        offset += ploidy_models[i]->Serialize(&dst[offset]);    
    }
    *((uint32_t*)&dst[0]) = offset;
    
    return offset;
}

int djinn_ctx_model::Serialize(std::ostream& stream) const {
    // Serialize as (int,uint32_t,uint32_t,uint8_t*,ctx1,ctx2):
    // #models,p_len,p,[models...]
    uint32_t out_len = GetSerializedSize();
    stream.write((char*)&out_len, sizeof(uint32_t));

    int n_models = ploidy_models.size();
    stream.write((char*)&n_models, sizeof(int));
    stream.write((char*)&n_variants, sizeof(uint32_t));

    // Serialize bit-packed controller.
    uint8_t pack = (use_pbwt << 7) | (init << 6) | (unused << 0);
    stream.write((char*)&pack, sizeof(uint8_t));
    stream.write((char*)&p_len, sizeof(uint32_t));
    stream.write((char*)p, p_len);

    // Serialize each model.
    for (int i = 0; i < ploidy_models.size(); ++i)
        ploidy_models[i]->Serialize(stream);

    return out_len;
}

int djinn_ctx_model::GetSerializedSize() const {
    int ret = sizeof(uint32_t) + sizeof(int) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + p_len;
    for (int i = 0; i < ploidy_models.size(); ++i) {
        ret += ploidy_models[i]->GetSerializedSize();
    }
    return ret;
}

int djinn_ctx_model::GetCurrentSize() const {
    int ret = range_coder->OutSize();
    for (int i = 0; i < ploidy_models.size(); ++i) {
        ret += ploidy_models[i]->GetCurrentSize();
    }
    return ret;
}

// Deserialize data from an external buffer.
int djinn_ctx_model::Deserialize(uint8_t* src) {
    // Reset.
    ploidy_remap.clear();

    uint32_t offset = 0;

    // Read total offset.
    uint32_t tot_offset = *((uint32_t*)&src[offset]);
    offset += sizeof(uint32_t);

    // Read #models,#variants,packed bools
    int n_models = *((int*)&src[offset]);
    offset += sizeof(int);
    n_variants = *((uint32_t*)&src[offset]);
    offset += sizeof(uint32_t);
    uint8_t pack = src[offset];
    use_pbwt = (pack >> 7) & 1;
    init = (pack >> 6) & 1;
    unused = 0;
    offset += sizeof(uint8_t);

    // Read p_len,p
    p_len = *((uint32_t*)&src[offset]);
    offset += sizeof(uint32_t);
    
    // Store model selection data.
    memcpy(p, &src[offset], p_len);
    offset += p_len;
    
    // Read each model.
    for (int i = 0; i < n_models; ++i) {
        // First peek at their content to invoke the correct constructor as it
        // requires #samples, #ploidy, use_pbwt
        int pl = *((int*)&src[offset]);
        uint32_t n_s = *((uint32_t*)&src[offset+sizeof(int)]);

        // Update ploidy map and insert data in the correct position relative
        // the encoding order.
        const uint64_t tuple = ((uint64_t)n_s << 32) | pl;
        auto search = ploidy_map.find(tuple);
        if (search != ploidy_map.end()) {
            offset += ploidy_models[search->second]->Deserialize(&src[offset]);
        } else {
            ploidy_map[tuple] = ploidy_models.size();
            ploidy_models.push_back(std::make_shared<djinn::djn_ctx_model_container_t>(n_s, pl, (bool)use_pbwt));
            offset += ploidy_models.back()->Deserialize(&src[offset]);
        }
    }
    // std::cerr << "[Deserialize] Decoded=" << offset << "/" << tot_offset << std::endl;
    assert(offset == tot_offset);
    return offset;
}

// Deserialize data from a IO stream. This approach is more efficient
// as data does NOT have to be copied and the current model can be
// reused.
int djinn_ctx_model::Deserialize(std::istream& stream) {
    // uint32_t out_len = GetSerializedSize();
    uint32_t out_len = 0;
    stream.read((char*)&out_len, sizeof(uint32_t));

    int n_models = 0;
    stream.read((char*)&n_models, sizeof(int));
    stream.read((char*)&n_variants, sizeof(uint32_t));

    // Deserialize bit-packed controller.
    uint8_t pack = 0;
    stream.read((char*)&pack, sizeof(uint8_t));
    use_pbwt = (pack >> 7) & 1;
    init = (pack >> 6) & 1;
    unused = 0;

    stream.read((char*)&p_len, sizeof(uint32_t));
    stream.read((char*)p, p_len);

    // Serialize each model.
    for (int i = 0; i < n_models; ++i) {
        // First peek at their content to invoke the correct constructor as it
        // requires #samples, #ploidy, use_pbwt
        int pl = 0; 
        uint32_t n_s = 0; 
        stream.read((char*)&pl, sizeof(int));
        stream.read((char*)&n_s, sizeof(uint32_t));

        // std::cerr << "[Deserialize] #pl=" << pl << ", #n_s=" << n_s << std::endl;

        // Update ploidy map and insert data in the correct position relative
        // the encoding order.
        const uint64_t tuple = ((uint64_t)n_s << 32) | pl;
        auto search = ploidy_map.find(tuple);
        if (search != ploidy_map.end()) {
            ploidy_models[search->second]->Deserialize(stream);
        } else {
            // std::cerr << "[Deserialize] Adding map [" << tuple << "] for [" << n_s << "," << pl << "]" << std::endl; 
            ploidy_map[tuple] = ploidy_models.size();
            ploidy_models.push_back(std::make_shared<djinn::djn_ctx_model_container_t>(n_s, pl, (bool)use_pbwt));
            ploidy_models.back()->Deserialize(stream);
        }
    }

    return stream.tellg();
}

/*======   Container   ======*/

djn_ctx_model_container_t::djn_ctx_model_container_t(int64_t n_s, int pl, bool use_pbwt) : 
    use_pbwt(use_pbwt),
    ploidy(pl), n_samples(n_s), n_variants(0),
    n_samples_wah(std::ceil((float)n_samples / 32) * 32), 
    n_samples_wah_nm(std::ceil((float)n_samples * 4/32) * 8),
    n_wah(n_samples_wah_nm / 8),
    wah_bitmaps(new uint32_t[n_wah]), 
    p(new uint8_t[1000000]), p_len(0), p_cap(1000000), p_free(true),
    range_coder(std::make_shared<RangeCoder>()), 
    marchetype(std::make_shared<GeneralModel>(2, 1024, range_coder)),
    model_2mc(std::make_shared<djn_ctx_model_t>()),
    model_nm(std::make_shared<djn_ctx_model_t>())
{
    assert(n_s % pl == 0); // #samples/#ploidy must be divisible
    model_2mc->Initiate2mc();
    model_nm->InitiateNm();
}

djn_ctx_model_container_t::djn_ctx_model_container_t(int64_t n_s, int pl, bool use_pbwt, uint8_t* src, uint32_t src_len) : 
    use_pbwt(use_pbwt),
    ploidy(pl), n_samples(n_s), n_variants(0),
    n_samples_wah(std::ceil((float)n_samples / 32) * 32), 
    n_samples_wah_nm(std::ceil((float)n_samples * 4/32) * 8),
    n_wah(n_samples_wah_nm / 8),
    wah_bitmaps(new uint32_t[n_wah]), 
    p(src), p_len(src_len), p_cap(0), p_free(false),
    range_coder(std::make_shared<RangeCoder>()), 
    marchetype(std::make_shared<GeneralModel>(2, 1024, range_coder)),
    model_2mc(std::make_shared<djn_ctx_model_t>()),
    model_nm(std::make_shared<djn_ctx_model_t>())
{
    assert(n_s % pl == 0); // #samples/#ploidy must be divisible
    model_2mc->Initiate2mc();
    model_nm->InitiateNm();
}

djn_ctx_model_container_t::~djn_ctx_model_container_t() {
    if (p_free) delete[] p;
    delete[] wah_bitmaps;
}

void djn_ctx_model_container_t::StartEncoding(bool use_pbwt, bool reset) {
    assert(range_coder.get() != nullptr);
    assert(model_2mc.get() != nullptr);
    assert(model_nm.get() != nullptr);

    if (use_pbwt) {
        if (model_2mc->pbwt->n_symbols == 0) {
            if (n_samples == 0) {
                std::cerr << "illegal: no sample number set!" << std::endl;
            }
            model_2mc->pbwt->Initiate(n_samples, 2);
        }

        if (model_nm->pbwt->n_symbols == 0) {
            if (n_samples == 0) {
                std::cerr << "illegal: no sample number set!" << std::endl;
            }
            model_nm->pbwt->Initiate(n_samples, 16);
        }
    }
    this->use_pbwt = use_pbwt;
    n_variants = 0;

    // Local range coder
    range_coder->SetOutput(p);
    range_coder->StartEncode();

    model_2mc->StartEncoding(use_pbwt, reset);
    model_nm->StartEncoding(use_pbwt, reset);
}

size_t djn_ctx_model_container_t::FinishEncoding() {
    if (range_coder.get() == nullptr) return -1;
    range_coder->FinishEncode();
    model_2mc->FinishEncoding();
    model_nm->FinishEncoding();
    p_len = range_coder->OutSize();

    size_t s_rc  = range_coder->OutSize();
    size_t s_2mc = model_2mc->range_coder->OutSize();
    size_t s_nm  = model_nm->range_coder->OutSize();
    
    return s_rc + s_2mc + s_nm;
}

void djn_ctx_model_container_t::StartDecoding(bool use_pbwt, bool reset) {
    if (use_pbwt) {
        if (model_2mc->pbwt->n_symbols == 0) {
            if (n_samples == 0) {
                std::cerr << "illegal: no sample number set!" << std::endl;
            }
            model_2mc->pbwt->Initiate(n_samples, 2);
        }

        if (model_nm->pbwt->n_symbols == 0) {
            if (n_samples == 0) {
                std::cerr << "illegal: no sample number set!" << std::endl;
            }
            model_nm->pbwt->Initiate(n_samples, 16);
        }
    }

    if (reset) {
        model_2mc->reset();
        model_nm->reset();
    } else {
        model_2mc->n_variants = 0;
        model_nm->n_variants  = 0;
    }

    this->use_pbwt = use_pbwt;

    // Local range coder
    if (range_coder.get() == nullptr)
        range_coder = std::make_shared<RangeCoder>();
    range_coder->SetInput(p);
    range_coder->StartDecode();

    model_2mc->StartDecoding(use_pbwt, reset);
    model_nm->StartDecoding(use_pbwt, reset);
}

int djn_ctx_model_container_t::Encode2mc(uint8_t* data, uint32_t len) {
    if (data == nullptr) return -1;
    if (n_samples == 0) return -2;

    if (wah_bitmaps == nullptr) {
        n_samples_wah = std::ceil((float)n_samples / 32) * 32;
        n_samples_wah_nm = std::ceil((float)n_samples * 4/32) * 8; // Can fit eight 4-bit entries in a 32-bit word
        n_wah = n_samples_wah_nm / 8; 
        wah_bitmaps = new uint32_t[n_wah];
    }
    
    memset(wah_bitmaps, 0, n_wah*sizeof(uint32_t));

    for (int i = 0; i < n_samples; ++i) {
        if (data[i]) {
            wah_bitmaps[i / 32] |= 1L << (i % 32);
        }
    }

    return EncodeWah(wah_bitmaps, n_samples_wah >> 5); // n_samples_wah / 32
}

int djn_ctx_model_container_t::Encode2mc(uint8_t* data, uint32_t len, const uint8_t* map, const int shift) {
    if (data == nullptr) return -1;
    if (n_samples == 0)  return -2;
    if (map == nullptr)  return -3;
    
    if (wah_bitmaps == nullptr) {
        n_samples_wah = std::ceil((float)n_samples / 32) * 32;
        n_samples_wah_nm = std::ceil((float)n_samples * 4/32) * 8; // Can fit eight 4-bit entries in a 32-bit word
        n_wah = n_samples_wah_nm / 8; 
        wah_bitmaps = new uint32_t[n_wah];
    }
    
    memset(wah_bitmaps, 0, n_wah*sizeof(uint32_t));

    for (int i = 0; i < n_samples; ++i) {
        if (map[data[i] >> shift]) {
            wah_bitmaps[i / 32] |= 1L << (i % 32);
        }
    }

    return EncodeWah(wah_bitmaps, n_samples_wah >> 5); // n_samples_wah / 32
}

int djn_ctx_model_container_t::EncodeNm(uint8_t* data, uint32_t len) {
    if (data == nullptr) return -1;
    if (n_samples == 0) return -2;
    
    if (wah_bitmaps == nullptr) {
        n_samples_wah = std::ceil((float)n_samples / 32) * 32;
        n_samples_wah_nm = std::ceil((float)n_samples * 4/32) * 8; // Can fit eight 4-bit entries in a 32-bit word
        n_wah = n_samples_wah_nm / 8; 
        wah_bitmaps = new uint32_t[n_wah];
    }
    
    memset(wah_bitmaps, 0, n_wah*sizeof(uint32_t));

    for (int i = 0; i < n_samples; ++i) {
        wah_bitmaps[i / 8] |= (uint32_t)data[i] << (4*(i % 8));
    }

    return EncodeWahNm(wah_bitmaps, n_samples_wah_nm >> 3); // n_samples_wah_nm / 8
}

int djn_ctx_model_container_t::EncodeNm(uint8_t* data, uint32_t len, const uint8_t* map, const int shift) {
    if (data == nullptr) return -1;
    if (n_samples == 0) return -2;
    if (map == nullptr) return -3;

    if (wah_bitmaps == nullptr) {
        n_samples_wah = std::ceil((float)n_samples / 32) * 32;
        n_samples_wah_nm = std::ceil((float)n_samples * 4/32) * 8; // Can fit eight 4-bit entries in a 32-bit word
        n_wah = n_samples_wah_nm / 8; 
        wah_bitmaps = new uint32_t[n_wah];
    }
    
    // Todo: move into ResetBitmaps() functions
    memset(wah_bitmaps, 0, n_wah*sizeof(uint32_t));

    for (int i = 0; i < n_samples; ++i) {
        wah_bitmaps[i / 8] |= (uint32_t)map[data[i] >> shift] << (4*(i % 8));
    }

    return EncodeWahNm(wah_bitmaps, n_samples_wah_nm >> 3); // n_samples_wah_nm / 8
}

int djn_ctx_model_container_t::DecodeRaw_nm(uint8_t* data, uint32_t& len) {
    if (data == nullptr) return -1;

    int64_t n_samples_obs = 0;
    int objects = 0;

    // Compute als
    memset(hist_alts, 0, 256*sizeof(uint32_t));

    // Emit empty EWAH marker.
    djinn_ewah_t* ewah = (djinn_ewah_t*)&data[len]; 
    ewah->reset();
    len += sizeof(djinn_ewah_t);

    while(true) {
        uint8_t type = model_nm->mtype->DecodeSymbol();
        if (type == 0) { // bitmaps
            ++ewah->dirty;
            uint32_t* t = (uint32_t*)&data[len];
            for (int i = 0; i < 4; ++i) {
                data[len] = model_nm->dirty_wah->DecodeSymbol();
                uint8_t r = data[len]; // copy
                ++len;
                for (int j = 0; j < 2; ++j) {
                    ++hist_alts[r & 15];
                    r >>= 4;
                }
            }
            
            n_samples_obs += 8;

        } else { // is RLE
            // Emit new EWAH marker
            if (ewah->clean > 0 || ewah->dirty > 0) {
                ewah = (djinn_ewah_t*)&data[len];
                ewah->reset();
                len += sizeof(djinn_ewah_t);
                ++objects;
            }

            // Decode an RLE
            uint32_t ref = 1; uint32_t len = 1;
            DecodeWahRLE_nm(ref, len, model_nm);
            ewah->ref   = ref & 15;
            ewah->clean = len;
            hist_alts[ref & 15] += len*8;

            n_samples_obs += ewah->clean*8;
        }

        if (n_samples_obs == n_samples_wah_nm) {
            // std::cerr << "[djn_ctx_model_container_t::DecodeRaw_nm] obs=" << n_samples_obs << "/" << n_samples_wah_nm << std::endl;
            break;
        }
        
        if (n_samples_obs > n_samples_wah_nm) {
            std::cerr << "[djn_ctx_model_container_t::DecodeRaw_nm] Decompression corruption: " << n_samples_obs << "/" << n_samples_wah_nm  << std::endl;
            exit(1);
        }
    }

    if (ewah->clean || ewah->dirty) {
        ++objects;
    }

    uint32_t n_alts_obs = 0;
    // std::cerr << "alts=";
    for (int i = 0; i < 256; ++i) {
        // if (hist_alts[i]) std::cerr << i << ":" << hist_alts[i] << ",";
        n_alts_obs += hist_alts[i];
    }
    // std::cerr << " n_alts_obs=" << n_alts_obs << "/" << n_samples_wah_nm << std::endl;
    assert(n_alts_obs == n_samples_wah_nm);

    return objects;
}

int djn_ctx_model_container_t::EncodeWah(uint32_t* wah, uint32_t len) { // input WAH-encoded data
    if (wah == nullptr) return -1;
    ++model_2mc->n_variants;

    uint32_t wah_ref = wah[0];
    uint32_t wah_run = 1;

    // Resize if necessary.
    if (model_2mc->range_coder->OutSize() + n_samples > model_2mc->p_cap) {
        const uint32_t rc_size = model_2mc->range_coder->OutSize();
        std::cerr << "[djn_ctx_model_container_t::EncodeWah][RESIZE] resizing from: " << rc_size << "->" << (rc_size + 2*n_samples + 65536) << std::endl;
        uint8_t* prev = model_2mc->p; // old
        model_2mc->p_cap = rc_size + 2*n_samples + 65536;
        model_2mc->p = new uint8_t[model_2mc->p_cap];
        memcpy(model_2mc->p, prev, rc_size);
        if (model_2mc->p_free) delete[] prev;
        model_2mc->p_free = true;
        model_2mc->range_coder->out_buf = model_2mc->p + rc_size; 
        model_2mc->range_coder->in_buf  = model_2mc->p;
    }

    for (int i = 1; i < len; ++i) {
        if ((wah_ref != 0 && wah_ref != std::numeric_limits<uint32_t>::max()) || (wah_ref != wah[i])) {
            if ((wah_ref != 0 && wah_ref != std::numeric_limits<uint32_t>::max()) || wah_run == 1) {
                model_2mc->mtype->EncodeSymbol(0);
                
                for (int i = 0; i < 4; ++i) {
                    model_2mc->dirty_wah->EncodeSymbol(wah_ref & 255);
                    wah_ref >>= 8;
                }
            } else {
                model_2mc->mtype->EncodeSymbol(1);
                EncodeWahRLE(wah_ref, wah_run, model_2mc);
            }
            wah_run = 0;
            wah_ref = wah[i];
        }
        ++wah_run;
    }

    if (wah_run) {
        if ((wah_ref != 0 && wah_ref != std::numeric_limits<uint32_t>::max()) || wah_run == 1) {
            model_2mc->mtype->EncodeSymbol(0);
            
            for (int i = 0; i < 4; ++i) {
                model_2mc->dirty_wah->EncodeSymbol(wah_ref & 255);
                wah_ref >>= 8;
            }
            
        } else {
            model_2mc->mtype->EncodeSymbol(1);
            EncodeWahRLE(wah_ref, wah_run, model_2mc);
        }
    }

    ++n_variants;
    return 1;
}

int djn_ctx_model_container_t::EncodeWahNm(uint32_t* wah, uint32_t len) { // input WAH-encoded data
    if (wah == nullptr) return -1;

    ++model_nm->n_variants;

    // Resize if necessary.
    // std::cerr << "[djn_ctx_model_container_t::EncodeWahNm] " << model_nm->range_coder->OutSize() << "/" << model_nm->p_cap << std::endl;
    if (model_nm->range_coder->OutSize() + n_samples > model_nm->p_cap) {
        const uint32_t rc_size = model_nm->range_coder->OutSize();
        std::cerr << "[djn_ctx_model_container_t::EncodeWahNm][RESIZE] resizing from: " 
            << rc_size << "->" << (rc_size + 2*n_samples + 65536) << std::endl;
        uint8_t* prev = model_nm->p; // old
        model_nm->p_cap = rc_size + 2*n_samples + 65536;
        model_nm->p = new uint8_t[model_nm->p_cap];
        memcpy(model_nm->p, prev, rc_size);
        if (model_nm->p_free) delete[] prev;
        model_nm->p_free = true;
        model_nm->range_coder->out_buf = model_nm->p + rc_size; 
        model_nm->range_coder->in_buf  = model_nm->p;
    }

    // Debug
    uint32_t n_objs = 1;
    uint32_t n_obs  = 0;
    
    djinn_ewah_t ewah; ewah.reset();
    uint32_t wah_ref = (wah[0] & 15);

    for (int i = 0; i < len; ++i) {
        // Build reference.
        wah_ref = (wah[i] & 15);

        // Is dirty
        if (wah[i] != DJN_NM_REF_BITS[wah_ref]) {
            if (ewah.clean) {
                model_nm->mtype->EncodeSymbol(1);
                EncodeWahRLE_nm(ewah.ref, ewah.clean, model_nm);
                n_obs += ewah.clean;
                ewah.reset();
            }

            ++ewah.dirty;
            model_nm->mtype->EncodeSymbol(0);
            
            wah_ref = wah[i];
            for (int i = 0; i < 4; ++i) {
                model_nm->dirty_wah->EncodeSymbol(wah_ref & 255);
                wah_ref >>= 8;
            }
            ++n_obs;
            ++n_objs;
        } 
        // Is clean
        else {
            // Dirty have been set then make new EWAH
            if (ewah.dirty) {
                if (ewah.clean) {
                    model_nm->mtype->EncodeSymbol(1);
                    EncodeWahRLE_nm(ewah.ref, ewah.clean, model_nm);
                    n_obs += ewah.clean;
                }

                ewah.reset();
                ++ewah.clean;
                ewah.ref = wah_ref;
                ++n_objs;
            } 
            // No dirty have been set
            else {
                if (ewah.clean == 0) ewah.ref = wah_ref;
                if (ewah.ref == wah_ref) ++ewah.clean;
                else { // Different clean word
                    if (ewah.clean) {
                        model_nm->mtype->EncodeSymbol(1);
                        EncodeWahRLE_nm(ewah.ref, ewah.clean, model_nm);
                        n_obs += ewah.clean;
                    }

                    ewah.reset();
                    ++ewah.clean;
                    ewah.ref = wah_ref;
                    ++n_objs;
                }
            }
        }
    }

    if (ewah.clean) {
        model_nm->mtype->EncodeSymbol(1);
        EncodeWahRLE_nm(ewah.ref, ewah.clean, model_nm);
        n_obs += ewah.clean;
        ++n_objs;
    }
    assert(n_obs == len);

    ++n_variants;
    return 1;
}

int djn_ctx_model_container_t::EncodeWahRLE(uint32_t ref, uint32_t len, std::shared_ptr<djn_ctx_model_t> model) {
    model->mref->EncodeSymbol(ref&1);
    uint32_t log_length = round_log2(len);
    model->mlog_rle->EncodeSymbol(log_length);

    if (log_length < 2) {
        // std::cerr << "single=" << len << "," << (ref&1) << std::endl;
    }
    else if (log_length <= 8) {
        assert(len < 256);
        model->mrle->model_context <<= 1;
        model->mrle->model_context |= (ref & 1);
        model->mrle->model_context <<= 5;
        model->mrle->model_context |= log_length;
        model->mrle->model_context &= model->mrle->model_ctx_mask;
        model->mrle->EncodeSymbolNoUpdate(len & 255);

    } else if (log_length <= 16) {
        assert(len < 65536);
        model->mrle2_1->model_context <<= 1;
        model->mrle2_1->model_context |= (ref & 1);
        model->mrle2_1->model_context <<= 5;
        model->mrle2_1->model_context |= log_length;
        model->mrle2_1->model_context &= model->mrle2_1->model_ctx_mask;
        model->mrle2_1->EncodeSymbolNoUpdate(len & 255);
        len >>= 8;
        model->mrle2_2->model_context <<= 1;
        model->mrle2_2->model_context |= (ref & 1);
        model->mrle2_2->model_context <<= 5;
        model->mrle2_2->model_context |= log_length;
        model->mrle2_2->model_context &= model->mrle2_2->model_ctx_mask;
        model->mrle2_2->EncodeSymbolNoUpdate(len & 255);
    } else {
        model->mrle4_1->model_context <<= 1;
        model->mrle4_1->model_context |= (ref & 1);
        model->mrle4_1->model_context <<= 5;
        model->mrle4_1->model_context |= log_length;
        model->mrle4_1->model_context &= model->mrle4_1->model_ctx_mask;
        model->mrle4_1->EncodeSymbolNoUpdate(len & 255);
        len >>= 8;
        model->mrle4_2->model_context <<= 1;
        model->mrle4_2->model_context |= (ref & 1);
        model->mrle4_2->model_context <<= 5;
        model->mrle4_2->model_context |= log_length;
        model->mrle4_2->model_context &= model->mrle4_2->model_ctx_mask;
        model->mrle4_2->EncodeSymbolNoUpdate(len & 255);
        len >>= 8;
        model->mrle4_3->model_context <<= 1;
        model->mrle4_3->model_context |= (ref & 1);
        model->mrle4_3->model_context <<= 5;
        model->mrle4_3->model_context |= log_length;
        model->mrle4_3->model_context &= model->mrle4_3->model_ctx_mask;
        model->mrle4_3->EncodeSymbolNoUpdate(len & 255);
        len >>= 8;
        model->mrle4_4->model_context <<= 1;
        model->mrle4_4->model_context |= (ref & 1);
        model->mrle4_4->model_context <<= 5;
        model->mrle4_4->model_context |= log_length;
        model->mrle4_4->model_context &= model->mrle4_4->model_ctx_mask;
        model->mrle4_4->EncodeSymbolNoUpdate(len & 255);
    }

    return 1;
}

int djn_ctx_model_container_t::EncodeWahRLE_nm(uint32_t ref, uint32_t len, std::shared_ptr<djn_ctx_model_t> model) {
    model->mref->EncodeSymbol(ref & 15);
    uint32_t log_length = round_log2(len);
    model->mlog_rle->EncodeSymbol(log_length);

    if (log_length < 2) {
        assert(len < 256);
        model->mrle->model_context = (ref & 15);
        model->mrle->model_context <<= 5;
        model->mrle->model_context |= log_length;
        model->mrle->model_context &= model->mrle->model_ctx_mask;
        model->mrle->EncodeSymbolNoUpdate(len & 255);
    }
    else if (log_length <= 8) {
        assert(len < 256);
        model->mrle->model_context = (ref & 15);
        model->mrle->model_context <<= 5;
        model->mrle->model_context |= log_length;
        model->mrle->model_context &= model->mrle->model_ctx_mask;
        model->mrle->EncodeSymbolNoUpdate(len & 255);

    } else if (log_length <= 16) {
        assert(len < 65536);
        model->mrle2_1->model_context = (ref & 15);
        model->mrle2_1->model_context <<= 5;
        model->mrle2_1->model_context |= log_length;
        model->mrle2_1->model_context &= model->mrle2_1->model_ctx_mask;
        model->mrle2_1->EncodeSymbolNoUpdate(len & 255);
        len >>= 8;
        model->mrle2_2->model_context = (ref & 15);
        model->mrle2_2->model_context <<= 5;
        model->mrle2_2->model_context |= log_length;
        model->mrle2_2->model_context &= model->mrle2_2->model_ctx_mask;
        model->mrle2_2->EncodeSymbolNoUpdate(len & 255);
    } else {
        model->mrle4_1->model_context = (ref & 15);
        model->mrle4_1->model_context <<= 5;
        model->mrle4_1->model_context |= log_length;
        model->mrle4_1->model_context &= model->mrle4_1->model_ctx_mask;
        model->mrle4_1->EncodeSymbolNoUpdate(len & 255);
        len >>= 8;
        model->mrle4_2->model_context = (ref & 15);
        model->mrle4_2->model_context <<= 5;
        model->mrle4_2->model_context |= log_length;
        model->mrle4_2->model_context &= model->mrle4_2->model_ctx_mask;
        model->mrle4_2->EncodeSymbolNoUpdate(len & 255);
        len >>= 8;
        model->mrle4_3->model_context = (ref & 15);
        model->mrle4_3->model_context <<= 5;
        model->mrle4_3->model_context |= log_length;
        model->mrle4_3->model_context &= model->mrle4_3->model_ctx_mask;
        model->mrle4_3->EncodeSymbolNoUpdate(len & 255);
        len >>= 8;
        model->mrle4_4->model_context = (ref & 15);
        model->mrle4_4->model_context <<= 5;
        model->mrle4_4->model_context |= log_length;
        model->mrle4_4->model_context &= model->mrle4_4->model_ctx_mask;
        model->mrle4_4->EncodeSymbolNoUpdate(len & 255);
    }

    return 1;
}

int djn_ctx_model_container_t::DecodeWahRLE(uint32_t& ref, uint32_t& len, std::shared_ptr<djn_ctx_model_t> model) {
    ref = model->mref->DecodeSymbol();
    uint32_t log_length = model->mlog_rle->DecodeSymbol();

    if (log_length < 2) {
        // std::cerr << "single=" << len << "," << (ref&1) << std::endl;
        exit(1);
    }
    else if (log_length <= 8) {
        model->mrle->model_context <<= 1;
        model->mrle->model_context |= (ref & 1);
        model->mrle->model_context <<= 5;
        model->mrle->model_context |= log_length;
        model->mrle->model_context &= model->mrle->model_ctx_mask;
        len = model->mrle->DecodeSymbolNoUpdate();
    } else if (log_length <= 16) {
        model->mrle2_1->model_context <<= 1;
        model->mrle2_1->model_context |= (ref & 1);
        model->mrle2_1->model_context <<= 5;
        model->mrle2_1->model_context |= log_length;
        model->mrle2_1->model_context &= model->mrle2_1->model_ctx_mask;
        len = model->mrle2_1->DecodeSymbolNoUpdate();
        model->mrle2_2->model_context <<= 1;
        model->mrle2_2->model_context |= (ref & 1);
        model->mrle2_2->model_context <<= 5;
        model->mrle2_2->model_context |= log_length;
        model->mrle2_2->model_context &= model->mrle2_2->model_ctx_mask;
        len |= (uint32_t)model->mrle2_2->DecodeSymbolNoUpdate() << 8;
    } else {
        model->mrle4_1->model_context <<= 1;
        model->mrle4_1->model_context |= (ref & 1);
        model->mrle4_1->model_context <<= 5;
        model->mrle4_1->model_context |= log_length;
        model->mrle4_1->model_context &= model->mrle4_1->model_ctx_mask;
        len = model->mrle4_1->DecodeSymbolNoUpdate();
        model->mrle4_2->model_context <<= 1;
        model->mrle4_2->model_context |= (ref & 1);
        model->mrle4_2->model_context <<= 5;
        model->mrle4_2->model_context |= log_length;
        model->mrle4_2->model_context &= model->mrle4_2->model_ctx_mask;
        len |= (uint32_t)model->mrle4_2->DecodeSymbolNoUpdate() << 8;
        model->mrle4_3->model_context <<= 1;
        model->mrle4_3->model_context |= (ref & 1);
        model->mrle4_3->model_context <<= 5;
        model->mrle4_3->model_context |= log_length;
        model->mrle4_3->model_context &= model->mrle4_3->model_ctx_mask;
        len |= (uint32_t)model->mrle4_3->DecodeSymbolNoUpdate() << 16;
        model->mrle4_4->model_context <<= 1;
        model->mrle4_4->model_context |= (ref & 1);
        model->mrle4_4->model_context <<= 5;
        model->mrle4_4->model_context |= log_length;
        model->mrle4_4->model_context &= model->mrle4_4->model_ctx_mask;
        len |= (uint32_t)model->mrle4_4->DecodeSymbolNoUpdate() << 24;
    }

    return 1;
}

int djn_ctx_model_container_t::DecodeWahRLE_nm(uint32_t& ref, uint32_t& len, std::shared_ptr<djn_ctx_model_t> model) {
    ref = model->mref->DecodeSymbol();
    uint32_t log_length = model->mlog_rle->DecodeSymbol();

    if (log_length < 2) {
        model->mrle->model_context = (ref & 15);
        model->mrle->model_context <<= 5;
        model->mrle->model_context |= log_length;
        model->mrle->model_context &= model->mrle->model_ctx_mask;
        len = model->mrle->DecodeSymbolNoUpdate();
    }
    else if (log_length <= 8) {
        model->mrle->model_context = (ref & 15);
        model->mrle->model_context <<= 5;
        model->mrle->model_context |= log_length;
        model->mrle->model_context &= model->mrle->model_ctx_mask;
        len = model->mrle->DecodeSymbolNoUpdate();
    } else if (log_length <= 16) {
        model->mrle2_1->model_context = (ref & 15);
        model->mrle2_1->model_context <<= 5;
        model->mrle2_1->model_context |= log_length;
        model->mrle2_1->model_context &= model->mrle2_1->model_ctx_mask;
        len = model->mrle2_1->DecodeSymbolNoUpdate();
        model->mrle2_2->model_context = (ref & 15);
        model->mrle2_2->model_context <<= 5;
        model->mrle2_2->model_context |= log_length;
        model->mrle2_2->model_context &= model->mrle2_2->model_ctx_mask;
        len |= (uint32_t)model->mrle2_2->DecodeSymbolNoUpdate() << 8;
    } else {
        model->mrle4_1->model_context = (ref & 15);
        model->mrle4_1->model_context <<= 5;
        model->mrle4_1->model_context |= log_length;
        model->mrle4_1->model_context &= model->mrle4_1->model_ctx_mask;
        len = model->mrle4_1->DecodeSymbolNoUpdate();
        model->mrle4_2->model_context = (ref & 15);
        model->mrle4_2->model_context <<= 5;
        model->mrle4_2->model_context |= log_length;
        model->mrle4_2->model_context &= model->mrle4_2->model_ctx_mask;
        len |= (uint32_t)model->mrle4_2->DecodeSymbolNoUpdate() << 8;
        model->mrle4_3->model_context = (ref & 15);
        model->mrle4_3->model_context <<= 5;
        model->mrle4_3->model_context |= log_length;
        model->mrle4_3->model_context &= model->mrle4_3->model_ctx_mask;
        len |= (uint32_t)model->mrle4_3->DecodeSymbolNoUpdate() << 16;
        model->mrle4_4->model_context = (ref & 15);
        model->mrle4_4->model_context <<= 5;
        model->mrle4_4->model_context |= log_length;
        model->mrle4_4->model_context &= model->mrle4_4->model_ctx_mask;
        len |= (uint32_t)model->mrle4_4->DecodeSymbolNoUpdate() << 24;
    }

    return 1;
}

int djn_ctx_model_container_t::DecodeNext(uint8_t* ewah_data, uint32_t& ret_ewah, uint8_t* ret_buffer, uint32_t& ret_len) {
    if (ewah_data == nullptr)  return -1;
    if (ret_buffer == nullptr) return -2;

    // Decode stream archetype.
    uint8_t type = marchetype->DecodeSymbol();

    size_t ret_ewah_init = ret_ewah;
    int objs = 0;
    switch(type) {
    case 0: objs = DecodeRaw(ewah_data, ret_ewah); break;
    case 1: objs = DecodeRaw_nm(ewah_data, ret_ewah); break;
    default: std::cerr << "[djn_ctx_model_container_t::DecodeNext] decoding error: " << (int)type << " (valid=[0,1])" << std::endl; return -1;
    }

    if (objs <= 0) return -1;

    if (use_pbwt) {
        if (type == 0) {
            if (hist_alts[1] >= 10) {
                model_2mc->pbwt->ReverseUpdateEWAH(ewah_data, ret_ewah, ret_buffer); 
                ret_len = n_samples;
            } else {
                // Unpack EWAH into literals according to current PPA
                uint32_t local_offset = ret_ewah_init;
                uint32_t ret_pos = 0;
                for (int j = 0; j < objs; ++j) {
                    djinn_ewah_t* ewah = (djinn_ewah_t*)&ewah_data[local_offset];
                    local_offset += sizeof(djinn_ewah_t);

                    // Clean words.
                    uint32_t to = ret_pos + ewah->clean*32 > n_samples ? n_samples : ret_pos + ewah->clean*32;
                    for (int i = ret_pos; i < to; ++i) {
                        ret_buffer[model_2mc->pbwt->ppa[i]] = (ewah->ref & 1);
                    }
                    ret_pos = to;

                    for (int i = 0; i < ewah->dirty; ++i) {
                        to = ret_pos + 32 > n_samples ? n_samples : ret_pos + 32;
                        
                        uint32_t dirty = *((uint32_t*)(&ewah_data[local_offset])); // copy
                        for (int j = ret_pos; j < to; ++j) {
                            ret_buffer[model_2mc->pbwt->ppa[j]] = (dirty & 1);
                            dirty >>= 1;
                        }
                        local_offset += sizeof(uint32_t);
                        ret_pos = to;
                    }
                    assert(ret_pos <= n_samples);
                    assert(local_offset <= ret_ewah);
                }
                assert(ret_pos == n_samples);
                ret_len = n_samples;
            }
        } else { // is NM
            model_nm->pbwt->ReverseUpdateEWAHNm(ewah_data, ret_ewah, ret_buffer);
            ret_len = n_samples;
        }
    } else { // not using PBWT
        if (type == 0) {
            // Unpack EWAH into literals
            uint32_t local_offset = ret_ewah_init;
            uint32_t ret_pos = 0;
            for (int j = 0; j < objs; ++j) {
                djinn_ewah_t* ewah = (djinn_ewah_t*)&ewah_data[local_offset];
                local_offset += sizeof(djinn_ewah_t);
                
                // Clean words.
                uint32_t to = ret_pos + ewah->clean*32 > n_samples ? n_samples : ret_pos + ewah->clean*32;
                for (int i = ret_pos; i < to; ++i) {
                    ret_buffer[i] = (ewah->ref & 1);
                }
                ret_pos = to;

                for (int i = 0; i < ewah->dirty; ++i) {
                    to = ret_pos + 32 > n_samples ? n_samples : ret_pos + 32;
                    
                    uint32_t dirty = *((uint32_t*)(&ewah_data[local_offset])); // copy
                    for (int j = ret_pos; j < to; ++j) {
                        ret_buffer[j] = (dirty & 1);
                        dirty >>= 1;
                    }
                    local_offset += sizeof(uint32_t);
                    ret_pos = to;
                }
                assert(ret_pos <= n_samples);
                assert(local_offset <= ret_ewah);
            }
            assert(ret_pos == n_samples);
            ret_len = n_samples;
        } else {
            // Unpack EWAH into literals
            uint32_t local_offset = ret_ewah_init;
            uint32_t n_s_obs = 0;

            for (int j = 0; j < objs; ++j) {
                djinn_ewah_t* ewah = (djinn_ewah_t*)&ewah_data[local_offset];
                local_offset += sizeof(djinn_ewah_t);

                // Clean words.
                uint64_t to = n_s_obs + ewah->clean * 8 > n_samples ? n_samples : n_s_obs + ewah->clean * 8;
                for (int i = n_s_obs; i < to; ++i) {
                    ret_buffer[i] = (ewah->ref & 15); // update prev when non-zero
                }
                n_s_obs = to;

                // Loop over dirty bitmaps.
                for (int i = 0; i < ewah->dirty; ++i) {
                    to = n_s_obs + 8 > n_samples ? n_samples : n_s_obs + 8;
                    assert(n_s_obs < n_samples);
                    assert(to <= n_samples);
                    
                    uint32_t dirty = *((uint32_t*)(&ewah_data[local_offset])); // copy
                    for (int j = n_s_obs; j < to; ++j) {
                        ret_buffer[j] = (dirty & 15);
                        dirty >>= 4;
                    }
                    local_offset += sizeof(uint32_t);
                    n_s_obs = to;
                }
                assert(local_offset <= ret_ewah);
            }
            assert(n_s_obs == n_samples);
            ret_len = n_samples;
        }
    }

    return objs;
}

int djn_ctx_model_container_t::DecodeNextRaw(uint8_t* data, uint32_t& len) {
    if (data == nullptr) return -1;
   
    // Decode stream archetype.
    uint8_t type = marchetype->DecodeSymbol();

    switch(type) {
    case 0: return(DecodeRaw(data, len)); break;
    case 1: return(DecodeRaw_nm(data, len)); break;
    default: std::cerr << "[djn_ctx_model_container_t::DecodeNextRaw] decoding error: " << (int)type << " (valid=[0,1])" << std::endl; return -1;
    }

    // Never reached.
    return -3;
}

int djn_ctx_model_container_t::DecodeNextRaw(djinn_variant_t*& variant) {
    if (variant == nullptr) {
        variant = new djinn_variant_t;
        variant->data_alloc = n_samples + 65536;
        variant->data = new uint8_t[variant->data_alloc];
        variant->data_free = true;
    } else if (n_samples >= variant->data_alloc) {
        if (variant->data_free) delete[] variant->data;
        variant->data_alloc = n_samples + 65536;
        variant->data = new uint8_t[variant->data_alloc];
        variant->data_free = true;
    }

    variant->ploidy   = ploidy;
    variant->data_len = 0;
    variant->errcode  = 0;
    variant->unpacked = DJN_UN_EWAH;
    variant->n_allele = 0;

    // Decode stream archetype.
    uint8_t type = marchetype->DecodeSymbol();

    int ret = 0;
    switch(type) {
    case 0: ret = DecodeRaw(variant->data, variant->data_len);    break;
    case 1: ret = DecodeRaw_nm(variant->data, variant->data_len); break;
    default: std::cerr << "[djn_ctx_model_container_t::DecodeNextRaw] decoding error: " << (int)type << " (valid=[0,1])" << std::endl; return -1;
    }

    if (ret <= 0) {
        variant->errcode = 1;
        return ret;
    }

    if (variant->d == nullptr) {
        variant->d = new djn_variant_dec_t;
    }

    if (ret >= variant->d->m_ewah) {
        delete[] variant->d->ewah;
        variant->d->ewah = new djinn_ewah_t*[ret + 512];
        variant->d->m_ewah = ret + 512;
    }

    if (variant->d->m_dirty < n_samples_wah_nm) {
        delete[] variant->d->dirty;
        variant->d->dirty = new uint32_t*[n_samples_wah_nm];
        variant->d->m_dirty = n_samples_wah_nm;
    }

    // Reset
    variant->d->n_ewah  = 0;
    variant->d->n_dirty = 0;
    // Set bitmap type.
    variant->d->dirty_type = type;
    variant->d->n_samples  = n_samples;

    // Construct EWAH mapping
    uint32_t local_offset = 0;
    uint32_t ret_pos = 0;

    for (int j = 0; j < ret; ++j) {
        djinn_ewah_t* ewah = (djinn_ewah_t*)&variant->data[local_offset];
        variant->d->ewah[variant->d->n_ewah++] = (djinn_ewah_t*)&variant->data[local_offset];
        local_offset += sizeof(djinn_ewah_t);
    
        // Clean words.
        uint32_t to = ret_pos + ewah->clean*32 > n_samples ? n_samples : ret_pos + ewah->clean*32;
        ret_pos = to;

        // Store first dirty pointer only.
        variant->d->dirty[variant->d->n_dirty++] = (uint32_t*)(&variant->data[local_offset]);

        for (int i = 0; i < ewah->dirty; ++i) {
            // variant->d->dirty[variant->d->n_dirty++] = (uint32_t*)(&variant->data[local_offset]);
            to = ret_pos + 32 > n_samples ? n_samples : ret_pos + 32;
            local_offset += sizeof(uint32_t);
            ret_pos = to;
        }
        assert(ret_pos <= n_samples);
        assert(local_offset <= variant->data_len);
    }
    assert(ret_pos == n_samples);

    // Compute number of alleles.
    uint32_t max_allele = 0;
    for (int i = 0; i < 256; ++i) {
        max_allele = hist_alts[i] != 0 ? i : max_allele;
        // if (tgt_container->hist_alts[i]) std::cerr << i << ":" << tgt_container->hist_alts[i] << ",";
    }
    // std::cerr << " max=" << max_allele << std::endl;
    variant->n_allele = max_allele + 1;

    return ret;
}

// Return raw, potentially permuted, EWAH encoding
int djn_ctx_model_container_t::DecodeRaw(uint8_t* data, uint32_t& len) {
    if (data == nullptr) return -1;

    int64_t n_samples_obs = 0;
    int objects = 0;

    // Emit empty EWAH marker.
    djinn_ewah_t* ewah = (djinn_ewah_t*)&data[len]; 
    ewah->reset();
    len += sizeof(djinn_ewah_t);

    // Compute als
    memset(hist_alts, 0, 256*sizeof(uint32_t));

    while(true) {
        uint8_t type = model_2mc->mtype->DecodeSymbol();

        if (type == 0) { // bitmaps
            ++ewah->dirty;

            uint32_t* c = (uint32_t*)&data[len]; // pointer to data
            for (int i = 0; i < 4; ++i) {
                data[len] = model_2mc->dirty_wah->DecodeSymbol();
                ++len;
            }
            hist_alts[0] += __builtin_popcount(~(*c));
            hist_alts[1] += __builtin_popcount(*c);
            n_samples_obs += 32;

        } else { // is RLE
            // Emit new EWAH marker
            if (ewah->clean > 0 || ewah->dirty > 0) {
                ewah = (djinn_ewah_t*)&data[len];
                ewah->reset();
                len += sizeof(djinn_ewah_t);
                ++objects;
            }

            // Decode an RLE
            uint32_t ref = 1; uint32_t len = 1;
            DecodeWahRLE(ref, len, model_2mc);
            ewah->ref = ref & 1;
            ewah->clean = len;
            hist_alts[ewah->ref] += ewah->clean * 32;

            n_samples_obs += len*32;
        }

        if (n_samples_obs == n_samples_wah) {
            // std::cerr << "obs=" << n_samples_obs << "/" << n_samples_wah << std::endl;
            break;
        }

        if (n_samples_obs > n_samples_wah) {
            std::cerr << "[djn_ctx_model_container_t::DecodeNextRaw] Decompression corruption: " << n_samples_obs << "/" << n_samples_wah  << std::endl;
            exit(1);
        }
    }

    if (ewah->clean > 0 || ewah->dirty > 0) {
        ++objects;
        // hist_alts[ewah->ref] += ewah->clean * 32;
    }

    uint32_t n_alts_obs = 0;
    // std::cerr << "alts=";
    for (int i = 0; i < 256; ++i) {
        // if (hist_alts[i]) std::cerr << i << ":" << hist_alts[i] << ",";
        n_alts_obs += hist_alts[i];
    }
    // std::cerr << " n_alts_obs=" << n_alts_obs << "/" << n_samples_wah << std::endl;
    assert(n_alts_obs == n_samples_wah);

    return objects;
}

int djn_ctx_model_container_t::Serialize(uint8_t* dst) const {
    // Serialize as (int,uint32_t,uint32_t,uint8_t*,ctx1,ctx2):
    // ploidy,n_samples,n_variants,p_len,p,model_2mc,model_nm
    uint32_t offset = 0;
    *((int*)&dst[offset]) = ploidy; // ploidy
    offset += sizeof(int);
    *((uint32_t*)&dst[offset]) = n_samples; // number of samples
    offset += sizeof(uint32_t);
    *((uint32_t*)&dst[offset]) = n_variants; // number of variants
    offset += sizeof(uint32_t);
    *((uint32_t*)&dst[offset]) = p_len; // data length
    offset += sizeof(uint32_t);
    memcpy(&dst[offset], p, p_len); // data
    offset += p_len;
    offset += model_2mc->Serialize(&dst[offset]);
    offset += model_nm->Serialize(&dst[offset]);
    return offset;
}

int djn_ctx_model_container_t::Serialize(std::ostream& stream) const {
    // Serialize as (int,uint32_t,uint32_t,uint8_t*,ctx1,ctx2):
    // ploidy,n_samples,n_variants,p_len,p,model_2mc,model_nm
    stream.write((char*)&ploidy, sizeof(int));
    stream.write((char*)&n_samples, sizeof(uint32_t));
    stream.write((char*)&n_variants, sizeof(uint32_t));
    stream.write((char*)&p_len, sizeof(uint32_t));
    stream.write((char*)p, p_len);
    model_2mc->Serialize(stream);
    model_nm->Serialize(stream);
    return stream.tellp();
}

int djn_ctx_model_container_t::GetSerializedSize() const {
    int ret = sizeof(int) + 3*sizeof(uint32_t) + p_len + model_2mc->GetSerializedSize() + model_nm->GetSerializedSize();
    return ret;
}

int djn_ctx_model_container_t::GetCurrentSize() const {
    int ret = range_coder->OutSize();
    ret += model_2mc->GetCurrentSize();
    ret += model_nm->GetCurrentSize();
    return ret;
}

int djn_ctx_model_container_t::Deserialize(uint8_t* dst) {
    uint32_t offset = 0;
    int pl = *((int*)&dst[offset]);
    // std::cerr << "pl=" << pl << " ploidy=" << ploidy << std::endl;
    assert(pl == ploidy);
    offset += sizeof(int);
    uint32_t n_s = *((uint32_t*)&dst[offset]);
    assert(n_s == n_samples);
    offset += sizeof(uint32_t);
    n_variants = *((uint32_t*)&dst[offset]);
    offset += sizeof(uint32_t);
    p_len = *((uint32_t*)&dst[offset]);
    offset += sizeof(uint32_t);

    // initiate a buffer if there is none or it's too small
    if (p_cap == 0 || p == nullptr || p_len > p_cap) {
        // std::cerr << "[Deserialize] Limit. p_cap=" << p_cap << "," << "p is nullptr=" << (p == nullptr ? "yes" : "no") << ",p_len=" << p_len << "/" << p_cap << std::endl;
        if (p_free) delete[] p;
        p = new uint8_t[p_len + 65536];
        p_cap = p_len + 65536;
        p_free = true;
    }

    memcpy(p, &dst[offset], p_len); // data
    offset += p_len;
    // Todo objects
    offset += model_2mc->Deserialize(&dst[offset]);
    offset += model_nm->Deserialize(&dst[offset]);

    return(offset);
}

int djn_ctx_model_container_t::Deserialize(std::istream& stream) {
    // #pl and #n_s read outside of this function in Deserialize() for
    // the parent.
    //
    // stream.read((char*)&ploidy, sizeof(int));
    // stream.read((char*)&n_samples, sizeof(uint32_t));

    stream.read((char*)&n_variants, sizeof(uint32_t));
    stream.read((char*)&p_len, sizeof(uint32_t));
    
    // initiate a buffer if there is none or it's too small
    if (p_cap == 0 || p == nullptr || p_len > p_cap) {
        // std::cerr << "[Deserialize] Limit. p_cap=" << p_cap << "," << "p is nullptr=" << (p == nullptr ? "yes" : "no") << ",p_len=" << p_len << "/" << p_cap << std::endl;
        if (p_free) delete[] p;
        p = new uint8_t[p_len + 65536];
        p_cap = p_len + 65536;
        p_free = true;
    }

    stream.read((char*)p, p_len);
    model_2mc->Deserialize(stream);
    model_nm->Deserialize(stream);
    return stream.tellg();
}

}