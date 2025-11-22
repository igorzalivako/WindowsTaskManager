#pragma once

#include "IGPUMonitor.h"
#include <QList>
#include <nvml.h>
#include <windows.h>
#include <adl_sdk.h>

typedef int(*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int(*ADL2_ADAPTER_NUMBEROFADAPTERS_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int (*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int (*ADL2_ADAPTER_ACTIVE_GET) (ADL_CONTEXT_HANDLE, int, int*);
typedef int (*ADL2_DISPLAY_MODES_GET)(ADL_CONTEXT_HANDLE, int iAdapterIndex, int iDisplayIndex, int* lpNumModes, ADLMode** lppModes);
typedef int (*ADL2_GRAPHICS_VERSIONSX2_GET)(ADL_CONTEXT_HANDLE context, ADLVersionsInfoX2* lpVersionsInfo);
typedef int (*ADL2_ADAPTER_ADAPTERINFO_GET)(ADL_CONTEXT_HANDLE 	context, AdapterInfo** lppInfo);
typedef int (*ADL2_OVERDRIVE5_CURRENTACTIVITY_GET)(ADL_CONTEXT_HANDLE context, int iAdapterIndex, ADLPMActivity* lpActivity);
typedef int (*ADL2_NEW_QUERYPMLOGDATA_GET)(ADL_CONTEXT_HANDLE context, int iAdapterIndex, ADLPMLogDataOutput* lpDataOutput);
typedef int (*ADL2_ADAPTER_MEMORYINFO_GET)(void*, int, ADLMemoryInfo2*);
typedef int (*ADL2_ADAPTER_VRAMUSAGE_GET)(void*, int, int*);

class WindowsGPUMonitor : public IGPUMonitor 
{
public:
    WindowsGPUMonitor() : nvmlInitialized(false), adlInitialized(false) { adlInitialized = initializeADL() && initializeNVML(); };
    ~WindowsGPUMonitor() {};
    QList<GPUInfo> getGPUInfo() override;
    QMap<quint32, ProcessGPUInfo> getProcessGPUInfo() override;

private:
    bool nvmlInitialized;
    bool adlInitialized;

    ADL_CONTEXT_HANDLE context = NULL;

    ADL2_MAIN_CONTROL_CREATE			ADL2_Main_Control_Create;
    ADL2_MAIN_CONTROL_DESTROY			ADL2_Main_Control_Destroy;
    ADL2_ADAPTER_NUMBEROFADAPTERS_GET	ADL2_Adapter_NumberOfAdapters_Get;
    ADL2_DISPLAY_MODES_GET				ADL2_Display_Modes_Get;
    ADL2_ADAPTER_ACTIVE_GET				ADL2_Adapter_Active_Get;
    ADL2_GRAPHICS_VERSIONSX2_GET        ADL2_Graphics_VersionsX2_Get;
    ADL2_ADAPTER_ADAPTERINFO_GET		ADL2_Adapter_AdapterInfo_Get;
    ADL2_OVERDRIVE5_CURRENTACTIVITY_GET ADL2_Overdrive5_CurrentActivity_Get;
    ADL2_NEW_QUERYPMLOGDATA_GET			ADL2_New_QueryPMLogDataGet;
    ADL2_ADAPTER_MEMORYINFO_GET			ADL2_Adapter_MemoryInfo_Get;
    ADL2_ADAPTER_VRAMUSAGE_GET			ADL2_Adapter_VRAMUsage_Get;

    QList<GPUInfo> getNvidiaGPUInfo();
    QList<GPUInfo> getAMDGPUInfo();

    bool initializeADL(); 
    bool initializeNVML();
    int GetAdapterActiveStatus(int adapterId, int& active) const;
    int GetUsedMemory(int adapterId) const;
};