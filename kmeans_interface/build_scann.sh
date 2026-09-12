# Copyright 2026 Google LLC

#!/bin/bash
set -e

# Change down to the interface directory
cd "$(dirname "$0")"
INTERFACE_DIR="$(pwd)"

echo "Setting up cached build directory..."
WORK_DIR="${TMPDIR:-/tmp}/kmeans_build_cache"
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

if [ ! -d "google-research/.git" ]; then
  echo "Cloning open-source google-research repo..."
  rm -rf google-research
  git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/google-research/google-research.git
  cd google-research
  git sparse-checkout set scann
else
  echo "Reusing cached google-research repository..."
  cd google-research
  git reset --hard HEAD
  git clean -fd
fi

echo "Injecting interface files into the cloned repository..."
# We place our interface files alongside the ScaNN source
mkdir -p scann/kmeans_interface
cp "$INTERFACE_DIR/scann_interface.h" scann/kmeans_interface/
cp "$INTERFACE_DIR/scann_interface.cc" scann/kmeans_interface/

# Create a local BUILD file inside the GitHub repository tree to build the .so
cat << 'EOF' > scann/kmeans_interface/BUILD
load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "libscann.so",
    srcs = ["scann_interface.cc", "scann_interface.h"],
    linkshared = True,
    copts = ["-fvisibility=hidden"],
    linkopts = ["-Wl,--exclude-libs,ALL"],
    deps = [
        "//scann/partitioning:partitioner_factory",
        "//scann/partitioning:partitioner_factory_base",
        "//scann/utils:threads",
        "//scann/utils:scalar_quantization_helpers",
        "//scann/data_format:dataset",
        "//scann/data_format:datapoint",
        "//scann/data_format:docid_collection",
        "//scann/distance_measures/one_to_one:dot_product",
        "//scann/proto:scann_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
EOF

echo "Downloading bazelisk to execute standard bazel build..."
BAZELISK_BASE="https://github.com/bazelbuild/bazelisk/releases/latest/download"
wget -qO bazel "$BAZELISK_BASE/bazelisk-linux-amd64"
chmod +x bazel

echo "Building libscann.so (Opaque ABI boundary) using open-source bazel..."
cd scann

echo "Patching GCC 15 strict vector assignment error in open source ScaNN..."
perl -pi -e \
  's/__m128\s+hi\s+=\s+_mm_srli_si128\(x,\s*8\);/'\
'__m128i hi = _mm_srli_si128((__m128i)x, 8);/g' \
  scann/utils/intrinsics/sse4.h
perl -pi -e \
  's/__m128\s+lo\s+=\s+x;/__m128i lo = (__m128i)x;/g' \
  scann/utils/intrinsics/sse4.h
perl -pi -e \
  's/_mm_cvtps_pd\(hi\)/_mm_cvtps_pd((__m128)hi)/g' \
  scann/utils/intrinsics/sse4.h
perl -pi -e \
  's/_mm_cvtps_pd\(lo\)/_mm_cvtps_pd((__m128)lo)/g' \
  scann/utils/intrinsics/sse4.h
grep -A2 "hi = " scann/utils/intrinsics/sse4.h || true

echo "Patching Clang-specific AMX intrinsics to compile on GCC 14/15..."
cat << 'EOF' > patch_amx.h
#if !defined(__clang__) && !defined(_SCANN_AMX_STUB_GUARD_)
#define _SCANN_AMX_STUB_GUARD_
struct __tile1024i { int a, b; };
static inline void __tile_loadd(void*, const void*, int) {}
static inline void __tile_zero(void*) {}
static inline void __tile_dpbssd(void*, __tile1024i, __tile1024i) {}
static inline void __tile_stored(void*, int, __tile1024i) {}
#endif
EOF
cat scann/distance_measures/many_to_many/int8_tile.h >> patch_amx.h
mv patch_amx.h scann/distance_measures/many_to_many/int8_tile.h

echo "Patching explicit float casts for brace init regressions in GCC 15..."
perl -0777 -pi -e \
  's/Sse4<float>\{\s*_mm_unpack(.*?)\};/'\
'Sse4<float>\(\(\(__m128\)_mm_unpack$1\)\);/gs' \
  scann/distance_measures/one_to_many/one_to_many_asymmetric_impl.inc
perl -0777 -pi -e \
  's/Avx2<float>\{\s*_mm256(.*?)\};/'\
'Avx2<float>\(\(\(__m256\)_mm256$1\)\);/gs' \
  scann/distance_measures/one_to_many/one_to_many_asymmetric_impl.inc

echo "Patching 2-phase template lookup failure in GCC 14..."
cat << 'EOF' > scann/distance_measures/one_to_many/patch_fwd.inc
template <typename T, typename DatasetView, typename Lambdas,
          typename ResultElem, bool kShouldPrefetch,
          typename CallbackFunctor>
enable_if_t<std::is_same_v<T, float>, void>
DenseAccumulatingDistanceMeasureOneToManyInternal(
    const DatapointPtr<T>& query, const DatasetView* __restrict__ database,
    const Lambdas& lambdas, MutableSpan<ResultElem> result,
    CallbackFunctor* __restrict__ callback, ThreadPool* pool);
template <typename T, typename DatasetView, typename Lambdas,
          typename ResultElem, bool kShouldPrefetch,
          typename CallbackFunctor>
enable_if_t<std::is_same_v<T, double>, void>
DenseAccumulatingDistanceMeasureOneToManyInternal(
    const DatapointPtr<T>& query, const DatasetView* __restrict__ database,
    const Lambdas& lambdas, MutableSpan<ResultElem> result,
    CallbackFunctor* __restrict__ callback, ThreadPool* pool);
EOF
PATCH_FWD_INC="scann/distance_measures/one_to_many/patch_fwd.inc"
sed -i \
  "/namespace one_to_many_low_level {/r $PATCH_FWD_INC" \
  scann/distance_measures/one_to_many/one_to_many_symmetric.h
perl -pi -e \
  's/return DenseAccumulatingDistanceMeasure/'\
'return one_to_many_low_level::DenseAccumulatingDistanceMeasure/g' \
  scann/distance_measures/one_to_many/one_to_many_symmetric.h

cat << 'EOF' > patch_fwd2.inc
template <typename T>
class SingleMachineSearcherBase;
template <typename T>
StatusOrSearcherUntyped RetrainAndReindexSearcherImpl(
    UntypedSingleMachineSearcherBase* untyped_searcher,
    absl::Mutex* searcher_pointer_mutex, ScannConfig config,
    shared_ptr<ThreadPool> parallelization_pool);
EOF
sed -i '/class SingleMachineSearcherBase;/r patch_fwd2.inc' \
  scann/base/single_machine_base.h

echo "Patching _mm_loadl_pi type violation in many_to_many_impl.inc..."
perl -0777 -pi -e '
  my $pat = qr/__m128i\s+int8s\s*=\s*_mm_loadl_pi\(\s*_mm_setzero_si128\(\),/ .
            qr/\s*reinterpret_cast<const\s+__m64\*>\(src\)\s*\);/;
  my $repl = "__m128i int8s = _mm_castps_si128(_mm_loadl_pi(" .
             "_mm_castsi128_ps(_mm_setzero_si128()), " .
             "reinterpret_cast<const __m64*>(src)));";
  s/$pat/$repl/gs;
' scann/distance_measures/many_to_many/many_to_many_impl.inc

echo "Patching static_cast<__m256> to _mm256_castsi256_ps..."
sed -i 's/static_cast<__m256>(mask)/_mm256_castsi256_ps(mask)/g' \
  scann/tree_x_hybrid/internal/utils.cc

echo "Patching dataset.h to expose set_docids_no_checks publicly..."
sed -i 's/protected:/public:/g' scann/data_format/dataset.h

echo "Patching missing typename in one_to_many_impl_highway.inc for GCC 15..."
sed -i 's/using Config\(.*\) = GenericConfig/using Config\1 = typename GenericConfig/g' \
  scann/distance_measures/one_to_many/one_to_many_impl_highway.inc
export USE_BAZEL_VERSION=7.4.1
mkdir -p "$WORK_DIR/bin"
ln -sf $(which python3) "$WORK_DIR/bin/python"
export PATH="$WORK_DIR/bin:$PATH"
../bazel build -c opt \
  --jobs="$(nproc)" \
  --disk_cache="$HOME/.cache/bazel_kmeans_disk" \
  --cxxopt="-std=c++17" --cxxopt="-fvisibility=hidden" --cxxopt="-fpermissive" \
  --cxxopt="-Wno-changes-meaning" --cxxopt="-Wno-template-body" \
  --cxxopt="-mavx" --cxxopt="-mavx2" --cxxopt="-mfma" --cxxopt="-msse4.1" \
  --cxxopt="-flax-vector-conversions" \
  --linkopt="-Wl,--exclude-libs,ALL" \
  //kmeans_interface:libscann.so

echo "Copying libscann.so to interface directory..."
cp bazel-bin/kmeans_interface/libscann.so "$INTERFACE_DIR/libscann.so"

echo "Extracting raw Protocol Buffer definitions..."
rm -rf "$INTERFACE_DIR/genproto"
mkdir -p "$INTERFACE_DIR/genproto"
cd "$WORK_DIR/google-research/scann"
find scann -name "*.proto" ! -path "*/scann_ops/*" | cpio -pdm "$INTERFACE_DIR/genproto/"

cd "$INTERFACE_DIR"

echo "Built successfully. libscann.so is available at:"
echo "  $INTERFACE_DIR/libscann.so"
