#include "linux_proc_cpuinfo.h"
#include <cxxtrace/detail/have.h> // IWYU pragma: keep
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>

using testing::ElementsAre;

namespace cxxtrace_test {
// TODO(strager): Write fuzz tests, especially if linux_proc_cpuinfo is moved
// into cxxtrace proper.

TEST(test_linux_proc_cpuinfo_x86_64, one_entry)
{
  auto cpuinfo_text =
    "processor\t: 0\n"
    "vendor_id\t: GenuineIntel\n"
    "cpu family\t: 6\n"
    "model\t\t: 142\n"
    "model name\t: Intel(R) Core(TM) i7-7500U CPU @ 2.70GHz\n"
    "stepping\t: 9\n"
    "microcode\t: 0xb4\n"
    "cpu MHz\t\t: 600.018\n"
    "cache size\t: 4096 KB\n"
    "physical id\t: 0\n"
    "siblings\t: 4\n"
    "core id\t\t: 0\n"
    "cpu cores\t: 2\n"
    "apicid\t\t: 0\n"
    "initial apicid\t: 0\n"
    "fpu\t\t: yes\n"
    "fpu_exception\t: yes\n"
    "cpuid level\t: 22\n"
    "wp\t\t: yes\n"
    "flags\t\t: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov "
    "pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx "
    "pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl "
    "xtopology nonstop_tsc cpuid aperfmperf tsc_known_freq pni pclmulqdq "
    "dtes64 monitor ds_cpl vmx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid "
    "sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c "
    "rdrand lahf_lm abm 3dnowprefetch cpuid_fault invpcid_single pti ssbd ibrs "
    "ibpb stibp tpr_shadow vnmi flexpriority ept vpid ept_ad fsgsbase "
    "tsc_adjust bmi1 avx2 smep bmi2 erms invpcid mpx rdseed adx smap "
    "clflushopt intel_pt xsaveopt xsavec xgetbv1 xsaves dtherm ida arat pln "
    "pts hwp hwp_notify hwp_act_window hwp_epp md_clear flush_l1d\n"
    "bugs\t\t: cpu_meltdown spectre_v1 spectre_v2 spec_store_bypass l1tf mds "
    "swapgs\n"
    "bogomips\t: 5808.00\n"
    "clflush size\t: 64\n"
    "cache_alignment\t: 64\n"
    "address sizes\t: 39 bits physical, 48 bits virtual\n"
    "power management:\n"
    "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);

  ASSERT_THAT(cpuinfo.get_processor_numbers(), ElementsAre(0));
  EXPECT_EQ(cpuinfo.get_initial_apicid(0), 0);
}

TEST(test_linux_proc_cpuinfo_x86_64, four_entries)
{
  auto cpuinfo_text =
    "processor\t: 0\n"
    "vendor_id\t: GenuineIntel\n"
    "cpu family\t: 6\n"
    "model\t\t: 142\n"
    "model name\t: Intel(R) Core(TM) i7-7500U CPU @ 2.70GHz\n"
    "stepping\t: 9\n"
    "microcode\t: 0xb4\n"
    "cpu MHz\t\t: 596.950\n"
    "cache size\t: 4096 KB\n"
    "physical id\t: 0\n"
    "siblings\t: 4\n"
    "core id\t\t: 0\n"
    "cpu cores\t: 2\n"
    "apicid\t\t: 0\n"
    "initial apicid\t: 0\n"
    "fpu\t\t: yes\n"
    "fpu_exception\t: yes\n"
    "cpuid level\t: 22\n"
    "wp\t\t: yes\n"
    "flags\t\t: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov "
    "pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx "
    "pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl "
    "xtopology nonstop_tsc cpuid aperfmperf tsc_known_freq pni pclmulqdq "
    "dtes64 monitor ds_cpl vmx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid "
    "sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c "
    "rdrand lahf_lm abm 3dnowprefetch cpuid_fault invpcid_single pti ssbd ibrs "
    "ibpb stibp tpr_shadow vnmi flexpriority ept vpid ept_ad fsgsbase "
    "tsc_adjust bmi1 avx2 smep bmi2 erms invpcid mpx rdseed adx smap "
    "clflushopt intel_pt xsaveopt xsavec xgetbv1 xsaves dtherm ida arat pln "
    "pts hwp hwp_notify hwp_act_window hwp_epp md_clear flush_l1d\n"
    "bugs\t\t: cpu_meltdown spectre_v1 spectre_v2 spec_store_bypass l1tf mds "
    "swapgs\n"
    "bogomips\t: 5808.00\n"
    "clflush size\t: 64\n"
    "cache_alignment\t: 64\n"
    "address sizes\t: 39 bits physical, 48 bits virtual\n"
    "power management:\n"
    "\n"
    "processor\t: 1\n"
    "vendor_id\t: GenuineIntel\n"
    "cpu family\t: 6\n"
    "model\t\t: 142\n"
    "model name\t: Intel(R) Core(TM) i7-7500U CPU @ 2.70GHz\n"
    "stepping\t: 9\n"
    "microcode\t: 0xb4\n"
    "cpu MHz\t\t: 586.570\n"
    "cache size\t: 4096 KB\n"
    "physical id\t: 0\n"
    "siblings\t: 4\n"
    "core id\t\t: 0\n"
    "cpu cores\t: 2\n"
    "apicid\t\t: 1\n"
    "initial apicid\t: 1\n"
    "fpu\t\t: yes\n"
    "fpu_exception\t: yes\n"
    "cpuid level\t: 22\n"
    "wp\t\t: yes\n"
    "flags\t\t: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov "
    "pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx "
    "pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl "
    "xtopology nonstop_tsc cpuid aperfmperf tsc_known_freq pni pclmulqdq "
    "dtes64 monitor ds_cpl vmx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid "
    "sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c "
    "rdrand lahf_lm abm 3dnowprefetch cpuid_fault invpcid_single pti ssbd ibrs "
    "ibpb stibp tpr_shadow vnmi flexpriority ept vpid ept_ad fsgsbase "
    "tsc_adjust bmi1 avx2 smep bmi2 erms invpcid mpx rdseed adx smap "
    "clflushopt intel_pt xsaveopt xsavec xgetbv1 xsaves dtherm ida arat pln "
    "pts hwp hwp_notify hwp_act_window hwp_epp md_clear flush_l1d\n"
    "bugs\t\t: cpu_meltdown spectre_v1 spectre_v2 spec_store_bypass l1tf mds "
    "swapgs\n"
    "bogomips\t: 5808.00\n"
    "clflush size\t: 64\n"
    "cache_alignment\t: 64\n"
    "address sizes\t: 39 bits physical, 48 bits virtual\n"
    "power management:\n"
    "\n"
    "processor\t: 2\n"
    "vendor_id\t: GenuineIntel\n"
    "cpu family\t: 6\n"
    "model\t\t: 142\n"
    "model name\t: Intel(R) Core(TM) i7-7500U CPU @ 2.70GHz\n"
    "stepping\t: 9\n"
    "microcode\t: 0xb4\n"
    "cpu MHz\t\t: 535.106\n"
    "cache size\t: 4096 KB\n"
    "physical id\t: 0\n"
    "siblings\t: 4\n"
    "core id\t\t: 1\n"
    "cpu cores\t: 2\n"
    "apicid\t\t: 3\n"
    "initial apicid\t: 3\n"
    "fpu\t\t: yes\n"
    "fpu_exception\t: yes\n"
    "cpuid level\t: 22\n"
    "wp\t\t: yes\n"
    "flags\t\t: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov "
    "pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx "
    "pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl "
    "xtopology nonstop_tsc cpuid aperfmperf tsc_known_freq pni pclmulqdq "
    "dtes64 monitor ds_cpl vmx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid "
    "sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c "
    "rdrand lahf_lm abm 3dnowprefetch cpuid_fault invpcid_single pti ssbd ibrs "
    "ibpb stibp tpr_shadow vnmi flexpriority ept vpid ept_ad fsgsbase "
    "tsc_adjust bmi1 avx2 smep bmi2 erms invpcid mpx rdseed adx smap "
    "clflushopt intel_pt xsaveopt xsavec xgetbv1 xsaves dtherm ida arat pln "
    "pts hwp hwp_notify hwp_act_window hwp_epp md_clear flush_l1d\n"
    "bugs\t\t: cpu_meltdown spectre_v1 spectre_v2 spec_store_bypass l1tf mds "
    "swapgs\n"
    "bogomips\t: 5808.00\n"
    "clflush size\t: 64\n"
    "cache_alignment\t: 64\n"
    "address sizes\t: 39 bits physical, 48 bits virtual\n"
    "power management:\n"
    "\n"
    "processor\t: 3\n"
    "vendor_id\t: GenuineIntel\n"
    "cpu family\t: 6\n"
    "model\t\t: 142\n"
    "model name\t: Intel(R) Core(TM) i7-7500U CPU @ 2.70GHz\n"
    "stepping\t: 9\n"
    "microcode\t: 0xb4\n"
    "cpu MHz\t\t: 569.750\n"
    "cache size\t: 4096 KB\n"
    "physical id\t: 0\n"
    "siblings\t: 4\n"
    "core id\t\t: 1\n"
    "cpu cores\t: 2\n"
    "apicid\t\t: 2\n"
    "initial apicid\t: 2\n"
    "fpu\t\t: yes\n"
    "fpu_exception\t: yes\n"
    "cpuid level\t: 22\n"
    "wp\t\t: yes\n"
    "flags\t\t: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov "
    "pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx "
    "pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl "
    "xtopology nonstop_tsc cpuid aperfmperf tsc_known_freq pni pclmulqdq "
    "dtes64 monitor ds_cpl vmx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid "
    "sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c "
    "rdrand lahf_lm abm 3dnowprefetch cpuid_fault invpcid_single pti ssbd ibrs "
    "ibpb stibp tpr_shadow vnmi flexpriority ept vpid ept_ad fsgsbase "
    "tsc_adjust bmi1 avx2 smep bmi2 erms invpcid mpx rdseed adx smap "
    "clflushopt intel_pt xsaveopt xsavec xgetbv1 xsaves dtherm ida arat pln "
    "pts hwp hwp_notify hwp_act_window hwp_epp md_clear flush_l1d\n"
    "bugs\t\t: cpu_meltdown spectre_v1 spectre_v2 spec_store_bypass l1tf mds "
    "swapgs\n"
    "bogomips\t: 5808.00\n"
    "clflush size\t: 64\n"
    "cache_alignment\t: 64\n"
    "address sizes\t: 39 bits physical, 48 bits virtual\n"
    "power management:\n"
    "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);

  ASSERT_THAT(cpuinfo.get_processor_numbers(), ElementsAre(0, 1, 2, 3));
  EXPECT_EQ(cpuinfo.get_initial_apicid(0), 0);
  EXPECT_EQ(cpuinfo.get_initial_apicid(1), 1);
  EXPECT_EQ(cpuinfo.get_initial_apicid(2), 3);
  EXPECT_EQ(cpuinfo.get_initial_apicid(3), 2);
}

TEST(test_linux_proc_cpuinfo_x86_64, entry_with_missing_initial_apicid)
{
  auto cpuinfo_text = "processor\t: 0\n"
                      "vendor_id\t: GenuineIntel\n"
                      "apicid\t\t: 0\n"
                      "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);
  ASSERT_THAT(cpuinfo.get_processor_numbers(), ElementsAre(0));
  EXPECT_EQ(cpuinfo.get_initial_apicid(0), std::nullopt);
}

TEST(test_linux_proc_cpuinfo_x86_64, out_of_range_processor_number_throws)
{
  auto cpuinfo_text =
    "processor\t: 0\n"
    "vendor_id\t: GenuineIntel\n"
    "cpu family\t: 6\n"
    "model\t\t: 142\n"
    "model name\t: Intel(R) Core(TM) i7-7500U CPU @ 2.70GHz\n"
    "stepping\t: 9\n"
    "microcode\t: 0xb4\n"
    "cpu MHz\t\t: 600.018\n"
    "cache size\t: 4096 KB\n"
    "physical id\t: 0\n"
    "siblings\t: 4\n"
    "core id\t\t: 0\n"
    "cpu cores\t: 2\n"
    "apicid\t\t: 0\n"
    "initial apicid\t: 0\n"
    "fpu\t\t: yes\n"
    "fpu_exception\t: yes\n"
    "cpuid level\t: 22\n"
    "wp\t\t: yes\n"
    "flags\t\t: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov "
    "pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx "
    "pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl "
    "xtopology nonstop_tsc cpuid aperfmperf tsc_known_freq pni pclmulqdq "
    "dtes64 monitor ds_cpl vmx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid "
    "sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c "
    "rdrand lahf_lm abm 3dnowprefetch cpuid_fault invpcid_single pti ssbd ibrs "
    "ibpb stibp tpr_shadow vnmi flexpriority ept vpid ept_ad fsgsbase "
    "tsc_adjust bmi1 avx2 smep bmi2 erms invpcid mpx rdseed adx smap "
    "clflushopt intel_pt xsaveopt xsavec xgetbv1 xsaves dtherm ida arat pln "
    "pts hwp hwp_notify hwp_act_window hwp_epp md_clear flush_l1d\n"
    "bugs\t\t: cpu_meltdown spectre_v1 spectre_v2 spec_store_bypass l1tf mds "
    "swapgs\n"
    "bogomips\t: 5808.00\n"
    "clflush size\t: 64\n"
    "cache_alignment\t: 64\n"
    "address sizes\t: 39 bits physical, 48 bits virtual\n"
    "power management:\n"
    "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);
  EXPECT_THROW({ cpuinfo.get_initial_apicid(1); }, std::out_of_range);
}

TEST(test_linux_proc_cpuinfo_x86_64, lines_before_first_processor_are_ignored)
{
  auto cpuinfo_text = "initial apicid\t: 1\n"
                      "\n"
                      "initial apicid\t: 2\n"
                      "processor\t: 0\n"
                      "initial apicid\t: 3\n"
                      "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);
  EXPECT_EQ(cpuinfo.get_initial_apicid(0), 3);
}

TEST(test_linux_proc_cpuinfo_x86_64, invalid_initial_apicid_line_is_ignored)
{
  auto cpuinfo_text = "processor\t: 0\n"
                      "initial apicid\t: flowers\n"
                      "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);
  EXPECT_EQ(cpuinfo.get_initial_apicid(0), std::nullopt);
}

TEST(test_linux_proc_cpuinfo_x86_64, empty_initial_apicid_line_is_ignored)
{
  auto cpuinfo_text = "processor\t: 0\n"
                      "initial apicid\t: \n"
                      "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);
  EXPECT_EQ(cpuinfo.get_initial_apicid(0), std::nullopt);
}

TEST(test_linux_proc_cpuinfo_x86_64, spurious_initial_apicid_lines_are_ignored)
{
  auto cpuinfo_text = "processor\t: 0\n"
                      "initial apicid\t: 1\n"
                      "initial apicid\t: 2\n"
                      "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);
  EXPECT_EQ(cpuinfo.get_initial_apicid(0), 1);
}

TEST(test_linux_proc_cpuinfo_x86_64, invalid_processor_lines_are_ignored)
{
  auto cpuinfo_text = "processor\t: 1\n"
                      "\n"
                      "processor\t: two\n"
                      "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);
  EXPECT_THAT(cpuinfo.get_processor_numbers(), ElementsAre(1));
}

TEST(test_linux_proc_cpuinfo_x86_64, lines_with_no_colon_are_ignored)
{
  auto cpuinfo_text = "processor\t: 1\n"
                      "\n"
                      "processor\t 2\n"
                      "initial apicid\t 2\n"
                      "\n";
  auto cpuinfo = linux_proc_cpuinfo::parse(cpuinfo_text);
  EXPECT_THAT(cpuinfo.get_processor_numbers(), ElementsAre(1));
  EXPECT_EQ(cpuinfo.get_initial_apicid(1), std::nullopt);
}

#if CXXTRACE_HAVE_LINUX_PROCFS
TEST(test_linux_proc_cpuinfo, real_cpuinfo_has_at_least_one_processor)
{
  auto cpuinfo = linux_proc_cpuinfo::parse();
  EXPECT_GE(cpuinfo.get_processor_numbers().size(), 1);
}
#endif
}
