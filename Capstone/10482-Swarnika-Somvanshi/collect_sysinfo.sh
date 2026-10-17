#!/usr/bin/env bash
# Saves the machine/toolchain details that go in the README. Run once: ./collect_sysinfo.sh
mkdir -p results
{
  echo "== date ==";                 date
  echo "== kernel ==";               uname -a
  echo "== os ==";                   grep PRETTY_NAME /etc/os-release
  echo "== cpu ==";                  lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core|L1d|L2|L3"
  echo "== P-core cpu ids ==";       cat /sys/devices/cpu_core/cpus
  echo "== compiler ==";             g++ --version | head -1
  echo "== perf ==";                 perf --version
  echo "== perf_event_paranoid =="; cat /proc/sys/kernel/perf_event_paranoid
  echo "== governor (cpu2) ==";      cat /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor 2>/dev/null || echo "n/a"
} > results/system_info.txt
echo "wrote results/system_info.txt"
