AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4
AM_CXXFLAGS = -fPIC -std=c++11
if ZSTD_AVAIL
AM_CXXFLAGS += -DHAVE_ZSTD
endif
if LZ4_AVAIL
AM_CXXFLAGS += -DHAVE_LZ4
endif
if ZLIB_AVAIL
AM_CXXFLAGS += -DHAVE_ZLIB
endif
if HTS_AVAIL
AM_CXXFLAGS += -DHAVE_HTS
endif

bin_PROGRAMS = djinn

djinn_SOURCES = main.cpp $(top_srcdir)/lib/djinn.h $(top_srcdir)/lib/vcf_reader.h $(top_srcdir)/examples/encode.h $(top_srcdir)/examples/htslib.h $(top_srcdir)/examples/iterate.h $(top_srcdir)/examples/iterate_raw.h $(top_srcdir)/examples/iterate_vcf.h
djinn_LDADD = libdjinn.la
djinn_CXXFLAGS = -I$(top_srcdir)/lib/ -std=c++11
if HAVE_ZLIB_PATH
djinn_CXXFLAGS += -L$(ZLIB_PATH)
endif
if HAVE_ZSTD_PATH
djinn_CXXFLAGS += -L$(ZSTD_PATH) -L$(ZSTD_PATH)/lib -I$(ZSTD_PATH) -I$(ZSTD_PATH)/lib -I$(ZSTD_PATH)/lib/common
endif
if HAVE_LZ4_PATH
djinn_CXXFLAGS += -L$(LZ4_PATH) -L$(LZ4_PATH)/lib -I$(LZ4_PATH) -I$(LZ4_PATH)/lib
endif
if HAVE_HTSLIB_PATH
djinn_CXXFLAGS += -L$(HTSLIB_PATH) -I$(HTSLIB_PATH)
endif

lib_LTLIBRARIES = libdjinn.la
libdjinn_la_LDFLAGS = -version-info 0:1:0
libdjinn_la_SOURCES = lib/compressors.h lib/ctx_model.cpp lib/djinn.cpp lib/djinn.h lib/ewah_model.cpp lib/frequency_model.cpp lib/frequency_model.h lib/pbwt.cpp lib/pbwt.h
libdjinn_ladir = $(includedir)/djinn
libdjinn_la_HEADERS = lib/djinn.h lib/vcf_reader.h