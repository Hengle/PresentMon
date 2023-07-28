#pragma once

#define COLUMN_GROUP_LIST \
X_(track_gpu, "Duration of each process' GPU work performed between presents. No Win7 support.") \
X_(track_gpu_video, "Duration of each process' video GPU work performed between presents. No Win7 support.") \
X_(track_input, "Time of keyboard/mouse clicks that were used by each frame.") \
X_(track_queue_timers, "Intel D3D11 driver producer/consumer queue timers.") \
X_(track_cpu_gpu_sync, "Intel D3D11 driver CPU/GPU syncs.") \
X_(track_shader_compilation, "Intel D3D11 driver shader compilation.") \
X_(track_memory_residency, "CPU time spent in memory residency and paging operations during each frame.") \
X_(track_gpu_telemetry, "GPU telemetry relating to power, temperature, frequency, clock speed, etc.") \
X_(track_vram_telemetry, "VRAM telemetry relating to power, temperature, frequency, etc.") \
X_(track_gpu_memory, "GPU memory utilization.") \
X_(track_gpu_fan, "GPU fanspeeds.") \
X_(track_gpu_psu, "GPU PSU information.") \
X_(track_perf_limit, "Flags denoting current reason for performance limitation.") \
X_(track_cpu_telemetry, "CPU telemetry relating to power, temperature, frequency, clock speed, etc.") \
X_(track_powershare_bias, "Powershare bias information.")