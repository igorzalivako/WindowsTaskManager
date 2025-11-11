#include "WindowsGPUMonitor.h"

QList<GPUInfo> WindowsGPUMonitor::getNvidiaGPUInfo() {
    QList<GPUInfo> gpus;

    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS) {
        return gpus;
    }

    unsigned int deviceCount = 0;
    result = nvmlDeviceGetCount(&deviceCount);
    if (result != NVML_SUCCESS || deviceCount == 0) {
        nvmlShutdown();
        return gpus;
    }

    for (unsigned int i = 0; i < deviceCount; i++) {
        nvmlDevice_t device;
        result = nvmlDeviceGetHandleByIndex(i, &device);
        if (result == NVML_SUCCESS) {
            GPUInfo gpu;
            gpu.vendor = "NVIDIA";

            // Получаем имя устройства
            char name[NVML_DEVICE_NAME_V2_BUFFER_SIZE];
            if (nvmlDeviceGetName(device, name, sizeof(name)) == NVML_SUCCESS) {
                gpu.name = name;
            }

            // Использование GPU
            nvmlUtilization_t utilization;
            if (nvmlDeviceGetUtilizationRates(device, &utilization) == NVML_SUCCESS) {
                gpu.usage = utilization.gpu;
            }

            // Информация о памяти
            nvmlMemory_t memory;
            if (nvmlDeviceGetMemoryInfo(device, &memory) == NVML_SUCCESS) {
                gpu.totalMemoryBytes = memory.total;
                gpu.usedMemoryBytes = memory.used;
            }

            // Температура
            unsigned int temp;
            if (nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
                gpu.temperatureCelsius = temp;
            }

            // Использование энергии
            unsigned int power;
            if (nvmlDeviceGetPowerUsage(device, &power) == NVML_SUCCESS) {
                gpu.powerUsage = power / 1000; // В ватты
            }

            // Версия драйвера
            char driver[80];
            if (nvmlSystemGetDriverVersion(driver, sizeof(driver)) == NVML_SUCCESS) {
                gpu.driverVersion = driver;
            }

            gpus.push_back(gpu);
        }
    }

    nvmlShutdown();
    return gpus;
}

void* __stdcall ADL_Main_Memory_Alloc(int iSize)
{
    void* lpBuffer = malloc(iSize);
    return lpBuffer;
}

void __stdcall ADL_Main_Memory_Free(void* lpBuffer)
{
    if (NULL != lpBuffer)
    {
        free(lpBuffer);
        lpBuffer = NULL;
    }
}

bool WindowsGPUMonitor::initializeADLX() {
    HINSTANCE hDLL;		// Handle to DLL
    hDLL = LoadLibrary(L"atiadlxx.dll");
    if (hDLL == nullptr)
    {
        return false;
    }

    ADL2_Adapter_NumberOfAdapters_Get = (ADL2_ADAPTER_NUMBEROFADAPTERS_GET)GetProcAddress(hDLL, "ADL2_Adapter_NumberOfAdapters_Get");
    ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE)GetProcAddress(hDLL, "ADL2_Main_Control_Create");
    ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY)GetProcAddress(hDLL, "ADL2_Main_Control_Destroy");
    ADL2_Display_Modes_Get = (ADL2_DISPLAY_MODES_GET)GetProcAddress(hDLL, "ADL2_Display_Modes_Get");
    ADL2_Adapter_Active_Get = (ADL2_ADAPTER_ACTIVE_GET)GetProcAddress(hDLL, "ADL2_Adapter_Active_Get");
    ADL2_Graphics_VersionsX2_Get = (ADL2_GRAPHICS_VERSIONSX2_GET)GetProcAddress(hDLL, "ADL2_Graphics_VersionsX2_Get");
    ADL2_Adapter_AdapterInfo_Get = (ADL2_ADAPTER_ADAPTERINFO_GET)GetProcAddress(hDLL, "ADL2_Adapter_AdapterInfoX2_Get");
    ADL2_Overdrive5_CurrentActivity_Get = (ADL2_OVERDRIVE5_CURRENTACTIVITY_GET)GetProcAddress(hDLL, "ADL2_Overdrive5_CurrentActivity_Get");
    ADL2_New_QueryPMLogDataGet = (ADL2_NEW_QUERYPMLOGDATA_GET)GetProcAddress(hDLL, "ADL2_New_QueryPMLogData_Get");
    ADL2_Adapter_MemoryInfo_Get = (ADL2_ADAPTER_MEMORYINFO_GET)GetProcAddress(hDLL, "ADL2_Adapter_MemoryInfo2_Get");
    ADL2_Adapter_VRAMUsage_Get = (ADL2_ADAPTER_VRAMUSAGE_GET)GetProcAddress(hDLL, "ADL2_Adapter_DedicatedVRAMUsage_Get");

    if (ADL_OK != ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &context))
    {
        return false;
    }

    return true;
}

int WindowsGPUMonitor::GetAdapterActiveStatus(int adapterId, int& active)
{
    active = 0;

    int result = ADL2_Adapter_Active_Get(context, adapterId, &active);

    return result;
}

#pragma runtime_checks("s", off)
int WindowsGPUMonitor::GetUsedMemory(int adapterId)
{
    int* vramUsage = new int;
    int result = ADL2_Adapter_VRAMUsage_Get(context, adapterId, vramUsage);
    return *vramUsage;
}
#pragma runtime_checks("s", on)

QList<GPUInfo> WindowsGPUMonitor::getAMDGPUInfo() {
    QList<GPUInfo> gpus;

    int  numberAdapters;

    if (ADL_OK != ADL2_Adapter_NumberOfAdapters_Get(context, &numberAdapters))
    {
        return gpus;
    }

    int active = 0;

    for (int i = 0; i < numberAdapters; i++) {
        if (ADL_OK == GetAdapterActiveStatus(i, active))
        {
            GPUInfo gpuInfo;
            if (active)
            {
                int numModes;
                ADLMode* adlMode;

                if (ADL_OK == ADL2_Display_Modes_Get(context, i, -1, &numModes, &adlMode))
                {
                    if (numModes == 1)
                    {
                        printf("Adapter %d resolution is %d by %d\n", i, adlMode->iXRes, adlMode->iYRes);
                        AdapterInfo** info = new AdapterInfo * ();
                        int result = ADL2_Adapter_AdapterInfo_Get(context, info);
                        ADLPMLogDataOutput* pmData = new ADLPMLogDataOutput();
                        result = ADL2_New_QueryPMLogDataGet(context, i, pmData);
                        ADLMemoryInfo2* meminfo = new ADLMemoryInfo2;
                        result = ADL2_Adapter_MemoryInfo_Get(context, i, meminfo);

                        gpuInfo.name = (*info)->strAdapterName;
                        gpuInfo.totalMemoryBytes = meminfo->iMemorySize;
                        gpuInfo.usage = pmData->sensors[ADL_PMLOG_GFX_CURRENT].value;
                        gpuInfo.usedMemoryBytes = GetUsedMemory(i) * 1024 * 1024;
                        gpuInfo.temperatureCelsius = pmData->sensors[ADL_PMLOG_TEMPERATURE_GFX].value;
                        gpuInfo.powerUsage = pmData->sensors[ADL_PMLOG_GFX_POWER].value;
                        gpuInfo.vendor = "AMD";
                        gpus.append(gpuInfo);
                        ADL_Main_Memory_Free(adlMode);
                    }
                }
            }
        }
    }

    return gpus;
}