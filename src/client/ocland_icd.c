/*
 *  This file is part of ocland, a free cloud OpenCL interface.
 *  Copyright (C) 2012  Jose Luis Cercos Pita <jl.cercos@upm.es>
 *
 *  ocland is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ocland is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ocland.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ocland/client/ocland_opencl.h>

#include <stdio.h>
#include <string.h>

#ifndef MAX_N_PLATFORMS
    #define MAX_N_PLATFORMS 1<<16 //   65536
#endif
#ifndef MAX_N_DEVICES
    #define MAX_N_DEVICES 1<<16   //   65536
#endif
#ifndef MAX_N_CONTEXTS
    #define MAX_N_CONTEXTS 1<<16  //   65536
#endif
#ifndef MAX_N_QUEUES
    #define MAX_N_QUEUES 1<<16    //   65536
#endif
#ifndef MAX_N_MEMS
    #define MAX_N_MEMS 1<<20      // 1048576
#endif
#ifndef MAX_N_PROGRAMS
    #define MAX_N_PROGRAMS 1<<20  // 1048576
#endif
#ifndef MAX_N_KERNELS
    #define MAX_N_KERNELS 1<<20   // 1048576
#endif
#ifndef MAX_N_EVENTS
    #define MAX_N_EVENTS 1<<20    // 1048576
#endif


#define SYMB(f) \
typeof(icd_##f) f __attribute__ ((alias ("icd_" #f), visibility("default")))

#pragma GCC visibility push(hidden)

cl_uint num_master_platforms = 0;
struct _cl_platform_id master_platforms[MAX_N_PLATFORMS];
cl_uint num_master_devices = 0;
struct _cl_device_id master_devices[MAX_N_DEVICES];
cl_uint num_master_contexts = 0;
cl_context master_contexts[MAX_N_CONTEXTS];
cl_uint num_master_queues = 0;
cl_command_queue master_queues[MAX_N_QUEUES];
cl_uint num_master_mems = 0;
cl_mem master_mems[MAX_N_MEMS];
cl_uint num_master_programs = 0;
cl_program master_programs[MAX_N_PROGRAMS];
cl_uint num_master_kernels = 0;
cl_kernel master_kernels[MAX_N_KERNELS];
cl_uint num_master_events = 0;
cl_event master_events[MAX_N_EVENTS];

// --------------------------------------------------------------
// Platforms
// --------------------------------------------------------------

static cl_int
__GetPlatformIDs(cl_uint num_entries,
                 cl_platform_id *platforms,
                 cl_uint *num_platforms)
{
    if(    ( !platforms   && !num_platforms )
        || (  num_entries && !platforms )
        || ( !num_entries &&  platforms ))
        return CL_INVALID_VALUE;
    cl_uint i;
    // Init platforms array
    if(!num_master_platforms){
        cl_int flag = oclandGetPlatformIDs(0,NULL,&num_master_platforms);
        if(flag != CL_SUCCESS){
            return flag;
        }
        if(!num_master_platforms){
            return CL_PLATFORM_NOT_FOUND_KHR;
        }
        cl_platform_id *server_platforms = (cl_platform_id*)malloc(num_master_platforms*sizeof(cl_platform_id));
        if(!server_platforms){
            return CL_OUT_OF_HOST_MEMORY;
        }
        flag = oclandGetPlatformIDs(num_master_platforms,server_platforms,NULL);
        if(flag != CL_SUCCESS){
            return flag;
        }
        // Send data to master_platforms
        for(i=0;i<num_master_platforms;i++){
            master_platforms[i].dispatch = &master_dispatch;
            master_platforms[i].ptr      = server_platforms[i];
        }
        free(server_platforms); server_platforms=NULL;
    }
    // Send requested data
    if( !num_master_platforms )
        return CL_PLATFORM_NOT_FOUND_KHR;
    if( num_platforms )
        *num_platforms = num_master_platforms;
    if( platforms ) {
        cl_uint i;
        for( i=0; i<(num_master_platforms<num_entries?num_master_platforms:num_entries); i++)
            platforms[i] = &master_platforms[i];
    }
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetPlatformIDs(cl_uint           num_entries ,
                     cl_platform_id *  platforms ,
                     cl_uint *         num_platforms) CL_API_SUFFIX__VERSION_1_0
{
    return __GetPlatformIDs(num_entries, platforms, num_platforms);
}
SYMB(clGetPlatformIDs);

CL_API_ENTRY cl_int CL_API_CALL
icd_clIcdGetPlatformIDsKHR(cl_uint num_entries,
                           cl_platform_id *platforms,
                           cl_uint *num_platforms) CL_API_SUFFIX__VERSION_1_2
{
    return __GetPlatformIDs(num_entries, platforms, num_platforms);
}
SYMB(clIcdGetPlatformIDsKHR);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetPlatformInfo(cl_platform_id   platform,
                      cl_platform_info param_name,
                      size_t           param_value_size,
                      void *           param_value,
                      size_t *         param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    if (param_value_size == 0 && param_value != NULL) {
        return CL_INVALID_VALUE;
    }
    // Connect to servers to get info
    return oclandGetPlatformInfo(platform->ptr, param_name, param_value_size, param_value, param_value_size_ret);
}
SYMB(clGetPlatformInfo);

// --------------------------------------------------------------
// Devices
// --------------------------------------------------------------

static cl_int
icd_clGetDeviceIDs(cl_platform_id   platform,
                 cl_device_type   device_type,
                 cl_uint          num_entries,
                 cl_device_id *   devices,
                 cl_uint *        num_devices)
{
    if( (!num_entries && devices) || (!devices && !num_devices) ){
        return CL_INVALID_VALUE;
    }
    cl_uint i,j,n;
    // Init devices array
    cl_int flag = oclandGetDeviceIDs(platform->ptr, device_type, 0, NULL, &n);
    if(flag != CL_SUCCESS){
        return flag;
    }
    cl_device_id *server_devices = (cl_device_id*)malloc(n*sizeof(cl_device_id));
    if(!server_devices){
        return CL_OUT_OF_HOST_MEMORY;
    }
    flag = oclandGetDeviceIDs(platform->ptr, device_type, n, server_devices, NULL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    for(i=0;i<n;i++){
        // Test if device has been already stored
        flag = CL_SUCCESS;
        for(j=0;j<num_master_devices;j++){
            if(master_devices[j].ptr == server_devices[i]){
                flag = CL_INVALID_DEVICE;
                break;
            }
        }
        if(flag != CL_SUCCESS)
            continue;
        // Add the new device
        num_master_devices++;
        if(num_master_devices > MAX_N_DEVICES){
            return CL_OUT_OF_RESOURCES;
        }
        master_devices[num_master_devices-1].dispatch = &master_dispatch;
        master_devices[num_master_devices-1].ptr      = server_devices[i];
    }
    // Send requested data
    if( num_devices != NULL )
        *num_devices = n;
    if( devices ) {
        for(i=0;i<(n<num_entries?n:num_entries);i++){
            for(j=0;j<num_master_devices;j++){
                if(master_devices[j].ptr == server_devices[i]){
                    devices[i] = &master_devices[j];
                    break;
                }
            }
        }
    }
    free(server_devices); server_devices=NULL;
    return CL_SUCCESS;
}
SYMB(clGetDeviceIDs);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetDeviceInfo(cl_device_id    device,
                    cl_device_info  param_name,
                    size_t          param_value_size,
                    void *          param_value,
                    size_t *        param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    cl_int flag = oclandGetDeviceInfo(device->ptr, param_name, param_value_size, param_value, param_value_size_ret);
    // If requested data is a platform, must be convinently corrected
    if((param_name == CL_DEVICE_PLATFORM) && param_value){
        cl_platform_id *platform = param_value;
        for(i=0;i<num_master_platforms;i++){
            if(master_platforms[i].ptr == *platform){
                *platform = (void*) &master_platforms[i];
                break;
            }
        }
    }
    return flag;
}
SYMB(clGetDeviceInfo);

CL_API_ENTRY cl_int CL_API_CALL
icd_clCreateSubDevices(cl_device_id                         in_device,
                       const cl_device_partition_property * properties,
                       cl_uint                              num_entries,
                       cl_device_id                       * out_devices,
                       cl_uint                            * num_devices) CL_API_SUFFIX__VERSION_1_2
{
    cl_uint i,n;
    if(    ( !out_devices && !num_devices )
        || ( !out_devices &&  num_entries )
        || (  out_devices && !num_entries ))
        num_entries = 0;
    // Count the number of properties
    cl_uint num_properties = 0;
    if(properties){
        while(properties[num_properties] != 0)
            num_properties++;
        num_properties++;   // Final zero must be counted
    }
    cl_int flag = oclandCreateSubDevices(in_device->ptr, properties, num_properties, num_entries, out_devices, &n);
    if(flag != CL_SUCCESS)
        return flag;
    if(num_devices)
        *num_devices = n;
    if(num_entries){
        n = n<num_entries?n:num_entries;
        for(i=0;i<n;i++){
            cl_device_id device = (cl_device_id)malloc(sizeof(struct _cl_device_id));
            if(!device){
                return CL_OUT_OF_HOST_MEMORY;
            }
            device->dispatch = &master_dispatch;
            device->ptr      = out_devices[i];
            num_master_devices++;
            master_devices[num_master_devices-1] = *device;
            out_devices[i]   = &master_devices[num_master_devices-1];
        }
    }
    return CL_SUCCESS;
}
SYMB(clCreateSubDevices);

CL_API_ENTRY cl_int CL_API_CALL
icd_clRetainDevice(cl_device_id device) CL_API_SUFFIX__VERSION_1_2
{
    return oclandRetainDevice(device->ptr);
}
SYMB(clRetainDevice);

CL_API_ENTRY cl_int CL_API_CALL
icd_clReleaseDevice(cl_device_id device) CL_API_SUFFIX__VERSION_1_2
{
    cl_uint i,j;
    cl_int flag = oclandReleaseDevice(device->ptr);
    if(flag != CL_SUCCESS)
        return flag;
    free(device);
    for(i=0;i<num_master_devices;i++){
        if(&master_devices[i] == device){
            for(j=i+1;j<num_master_devices;j++)
                master_devices[j-1] = master_devices[j];
            break;
        }
    }
    num_master_devices--;
    return CL_SUCCESS;
}
SYMB(clReleaseDevice);

// --------------------------------------------------------------
// Context
// --------------------------------------------------------------

CL_API_ENTRY cl_context CL_API_CALL
icd_clCreateContext(const cl_context_properties * properties,
                    cl_uint                       num_devices ,
                    const cl_device_id *          devices,
                    void (CL_CALLBACK * pfn_notify)(const char *, const void *, size_t, void *),
                    void *                        user_data,
                    cl_int *                      errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    /** callbacks can't be implemented trought network, so
     * if you request a callback CL_OUT_OF_RESOURCES will
     * be reported.
     */
    if(pfn_notify || user_data){
        if(errcode_ret) *errcode_ret = CL_OUT_OF_RESOURCES;
        return NULL;
    }
    // validate the arguments
    if( !num_devices || !devices || (!pfn_notify && user_data) ){
        if(errcode_ret) *errcode_ret = CL_INVALID_VALUE;
        return NULL;
    }
    // Count the number of properties
    cl_uint i, num_properties = 0;
    if(properties){
        while(properties[num_properties] != 0)
            num_properties++;
        num_properties++;   // Final zero must be counted
    }
    // Look for platform property that must be corrected
    cl_context_properties *props = (cl_context_properties*)properties;
    for(i=0;i<num_properties-1;i++){
        if(properties[i] == CL_CONTEXT_PLATFORM){
            props[i+1] = (cl_context_properties)((cl_platform_id)(properties[i+1]))->ptr;
        }
    }
    // Correct devices
    cl_device_id devs[num_devices];
    for(i=0;i<num_devices;i++){
        devs[i] = devices[i]->ptr;
    }
    cl_context context = (cl_context)malloc(sizeof(struct _cl_context));
    if(!context){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    context->dispatch = &master_dispatch;
    context->ptr = oclandCreateContext(properties, num_properties, num_devices, devs, NULL, NULL, errcode_ret);
    num_master_contexts++;
    master_contexts[num_master_contexts-1] = context;
    return context;
}
SYMB(clCreateContext);

CL_API_ENTRY cl_context CL_API_CALL
icd_clCreateContextFromType(const cl_context_properties * properties,
                            cl_device_type                device_type,
                            void (CL_CALLBACK *     pfn_notify)(const char *, const void *, size_t, void *),
                            void *                        user_data,
                            cl_int *                      errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    /** callbacks can't be implemented trought network, so
     * if you request a callback CL_OUT_OF_RESOURCES will
     * be reported.
     */
    if(pfn_notify || user_data){
        if(errcode_ret) *errcode_ret = CL_OUT_OF_RESOURCES;
        return NULL;
    }
    if(!pfn_notify && user_data){
        if(errcode_ret) *errcode_ret = CL_INVALID_VALUE;
        return NULL;
    }
    // Count the number of properties
    cl_uint i,num_properties = 0;
    if(properties){
        while(properties[num_properties] != 0)
            num_properties++;
        num_properties++;   // Final zero must be counted
    }
    // Look for platform property that must be corrected
    cl_context_properties *props = (cl_context_properties*)properties;
    for(i=0;i<num_properties-1;i++){
        if(properties[i] == CL_CONTEXT_PLATFORM){
            props[i+1] = (cl_context_properties)((cl_platform_id)(properties[i+1]))->ptr;
        }
    }
    cl_context context = (cl_context)malloc(sizeof(struct _cl_context));
    if(!context){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    context->dispatch = &master_dispatch;
    context->ptr = oclandCreateContextFromType(properties, num_properties, device_type, NULL, NULL, errcode_ret);
    master_contexts[num_master_contexts-1] = context;
    return context;
}
SYMB(clCreateContextFromType);

CL_API_ENTRY cl_int CL_API_CALL
icd_clRetainContext(cl_context context) CL_API_SUFFIX__VERSION_1_0
{
    return oclandRetainContext(context->ptr);
}
SYMB(clRetainContext);

CL_API_ENTRY cl_int CL_API_CALL
icd_clReleaseContext(cl_context context) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,j;
    cl_int flag = oclandReleaseContext(context->ptr);
    free(context);
    for(i=0;i<num_master_contexts;i++){
        if(master_contexts[i] == context){
            for(j=i+1;j<num_master_contexts;j++)
                master_contexts[j-1] = master_contexts[j];
            break;
        }
    }
    num_master_contexts--;
    return flag;
}
SYMB(clReleaseContext);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetContextInfo(cl_context         context,
                     cl_context_info    param_name,
                     size_t             param_value_size,
                     void *             param_value,
                     size_t *           param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,j,n;
    cl_int flag = oclandGetContextInfo(context->ptr, param_name, param_value_size, param_value, param_value_size_ret);
    // If requested data is the devices, must be convinently corrected
    if((param_name == CL_CONTEXT_DEVICES) && param_value){
        n = param_value_size / sizeof(cl_device_id);
        for(i=0;i<n;i++){
            cl_device_id *device = param_value + i*sizeof(cl_device_id);
            for(j=0;j<num_master_devices;j++){
                if(master_devices[j].ptr == *device){
                    *device = (void*) &master_devices[j];
                    break;
                }
            }
        }
    }
    // If requested data is the context properties, platform can be inside, and then must be corrected
    if((param_name == CL_CONTEXT_PROPERTIES) && param_value){
        n = param_value_size / sizeof(cl_context_properties);
        cl_context_properties *properties = param_value;
        for(i=0;i<n;i++){
            if(properties[i] == CL_CONTEXT_PLATFORM){
                properties[i+1] = (cl_context_properties)((cl_platform_id)(properties[i+1]))->ptr;
            }
        }
    }
    return flag;
}
SYMB(clGetContextInfo);

// --------------------------------------------------------------
// Command Queue
// --------------------------------------------------------------

CL_API_ENTRY cl_command_queue CL_API_CALL
icd_clCreateCommandQueue(cl_context                     context,
                         cl_device_id                   device,
                         cl_command_queue_properties    properties,
                         cl_int *                       errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_command_queue queue = (cl_command_queue)malloc(sizeof(struct _cl_command_queue));
    if(!queue){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    queue->dispatch = &master_dispatch;
    queue->ptr = oclandCreateCommandQueue(context->ptr,device->ptr,properties,errcode_ret);
    num_master_queues++;
    master_queues[num_master_queues-1] = queue;
    return queue;
}
SYMB(clCreateCommandQueue);

CL_API_ENTRY cl_int CL_API_CALL
icd_clRetainCommandQueue(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
    return oclandRetainCommandQueue(command_queue->ptr);
}
SYMB(clRetainCommandQueue);

CL_API_ENTRY cl_int CL_API_CALL
icd_clReleaseCommandQueue(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,j;
    cl_int flag = oclandReleaseCommandQueue(command_queue->ptr);
    free(command_queue);
    for(i=0;i<num_master_queues;i++){
        if(master_queues[i] == command_queue){
            for(j=i+1;j<num_master_queues;j++)
                master_queues[j-1] = master_queues[j];
            break;
        }
    }
    num_master_queues--;
    return flag;
}
SYMB(clReleaseCommandQueue);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetCommandQueueInfo(cl_command_queue      command_queue,
                          cl_command_queue_info param_name,
                          size_t                param_value_size,
                          void *                param_value,
                          size_t *              param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    cl_int flag = oclandGetCommandQueueInfo(command_queue->ptr,param_name,param_value_size,param_value,param_value_size_ret);
    // If requested data is a context, must be convinently corrected
    if((param_name == CL_QUEUE_CONTEXT) && param_value){
        cl_context *context = param_value;
        for(i=0;i<num_master_contexts;i++){
            if(master_contexts[i]->ptr == *context){
                *context = (void*) master_contexts[i];
                break;
            }
        }
    }
    // If requested data is a device, must be convinently corrected
    if((param_name == CL_QUEUE_DEVICE) && param_value){
        cl_device_id *device = param_value;
        for(i=0;i<num_master_devices;i++){
            if(master_devices[i].ptr == *device){
                *device = (void*) &master_devices[i];
                break;
            }
        }
    }
    return flag;
}
SYMB(clGetCommandQueueInfo);

// --------------------------------------------------------------
// Memory objects
// --------------------------------------------------------------

CL_API_ENTRY cl_mem CL_API_CALL
icd_clCreateBuffer(cl_context    context ,
                   cl_mem_flags  flags ,
                   size_t        size ,
                   void *        host_ptr ,
                   cl_int *      errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    /** CL_MEM_USE_HOST_PTR and CL_MEM_ALLOC_HOST_PTR are unusable
     * along network, so if detected CL_INVALID_VALUE will
     * returned. In future developments a walking around method can
     * be drafted.
     */
    if( (flags & CL_MEM_USE_HOST_PTR) || (flags & CL_MEM_ALLOC_HOST_PTR) ){
        if(errcode_ret) *errcode_ret=CL_INVALID_VALUE;
        return NULL;
    }

    if(host_ptr && ( !(flags & CL_MEM_COPY_HOST_PTR) && !(flags & CL_MEM_USE_HOST_PTR) )){
        if(errcode_ret) *errcode_ret=CL_INVALID_HOST_PTR;
        return NULL;
    }
    else if(!host_ptr && ( (flags & CL_MEM_USE_HOST_PTR) || (flags & CL_MEM_COPY_HOST_PTR) )){
        if(errcode_ret) *errcode_ret=CL_INVALID_HOST_PTR;
        return NULL;
    }

    cl_mem mem_obj = (cl_mem)malloc(sizeof(struct _cl_mem));
    if(!mem_obj){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    mem_obj->dispatch = &master_dispatch;
    mem_obj->ptr = oclandCreateBuffer(context->ptr, flags, size, host_ptr, errcode_ret);
    num_master_mems++;
    master_mems[num_master_mems-1] = mem_obj;
    return mem_obj;
}
SYMB(clCreateBuffer);

CL_API_ENTRY cl_int CL_API_CALL
icd_clRetainMemObject(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0
{
    return oclandRetainMemObject(memobj->ptr);
}
SYMB(clRetainMemObject);

CL_API_ENTRY cl_int CL_API_CALL
icd_clReleaseMemObject(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,j;
    cl_int flag = oclandReleaseMemObject(memobj->ptr);
    free(memobj);
    for(i=0;i<num_master_mems;i++){
        if(master_mems[i] == memobj){
            for(j=i+1;j<num_master_mems;j++)
                master_mems[j-1] = master_mems[j];
            break;
        }
    }
    num_master_mems--;
    return flag;
}
SYMB(clReleaseMemObject);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetSupportedImageFormats(cl_context           context,
                               cl_mem_flags         flags,
                               cl_mem_object_type   image_type ,
                               cl_uint              num_entries ,
                               cl_image_format *    image_formats ,
                               cl_uint *            num_image_formats) CL_API_SUFFIX__VERSION_1_0
{
    if(!num_entries && image_formats){
        return CL_INVALID_VALUE;
    }
    return oclandGetSupportedImageFormats(context->ptr,flags,image_type,num_entries,image_formats,num_image_formats);
}
SYMB(clGetSupportedImageFormats);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetMemObjectInfo(cl_mem            memobj ,
                       cl_mem_info       param_name ,
                       size_t            param_value_size ,
                       void *            param_value ,
                       size_t *          param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    cl_int flag = oclandGetMemObjectInfo(memobj->ptr,param_name,param_value_size,param_value,param_value_size_ret);
    // If requested data is a context, must be convinently corrected
    if((param_name == CL_MEM_CONTEXT) && param_value){
        cl_context *context = param_value;
        for(i=0;i<num_master_contexts;i++){
            if(master_contexts[i]->ptr == *context){
                *context = (void*) master_contexts[i];
                break;
            }
        }
    }
    // If requested data is a memory object, must be convinently corrected
    if((param_name == CL_MEM_ASSOCIATED_MEMOBJECT) && param_value){
        cl_mem *mem = param_value;
        for(i=0;i<num_master_mems;i++){
            if(master_mems[i]->ptr == *mem){
                *mem = (void*) master_mems[i];
                break;
            }
        }
    }
    return flag;
}
SYMB(clGetMemObjectInfo);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetImageInfo(cl_mem            image ,
                   cl_image_info     param_name ,
                   size_t            param_value_size ,
                   void *            param_value ,
                   size_t *          param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    return oclandGetImageInfo(image->ptr,param_name,param_value_size,param_value,param_value_size_ret);
}
SYMB(clGetImageInfo);

CL_API_ENTRY cl_mem CL_API_CALL
icd_clCreateSubBuffer(cl_mem                    buffer ,
                  cl_mem_flags              flags ,
                  cl_buffer_create_type     buffer_create_type ,
                  const void *              buffer_create_info ,
                  cl_int *                  errcode_ret) CL_API_SUFFIX__VERSION_1_1
{
    /** CL_MEM_USE_HOST_PTR and CL_MEM_ALLOC_HOST_PTR are unusable
     * along network, so if detected CL_INVALID_VALUE will
     * returned. In future developments a walking around method can
     * be drafted.
     */
    if( (flags & CL_MEM_USE_HOST_PTR) || (flags & CL_MEM_ALLOC_HOST_PTR) ){
        if(errcode_ret) *errcode_ret=CL_INVALID_VALUE;
        return NULL;
    }

    cl_mem mem_obj = (cl_mem)malloc(sizeof(struct _cl_mem));
    if(!mem_obj){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    mem_obj->dispatch = &master_dispatch;
    mem_obj->ptr = oclandCreateSubBuffer(buffer->ptr, flags, buffer_create_type, buffer_create_info, errcode_ret);
    num_master_mems++;
    master_mems[num_master_mems-1] = mem_obj;
    return mem_obj;
}
SYMB(clCreateSubBuffer);

CL_API_ENTRY cl_int CL_API_CALL
icd_clSetMemObjectDestructorCallback(cl_mem  memobj ,
                                 void (CL_CALLBACK * pfn_notify)(cl_mem  memobj , void* user_data),
                                 void * user_data)             CL_API_SUFFIX__VERSION_1_1
{
    /** Callbacks can't be registered in ocland due
     * to the implicit network interface, so this
     * operation may fail ever.
     */
    return CL_INVALID_MEM_OBJECT;
}
SYMB(clSetMemObjectDestructorCallback);

CL_API_ENTRY cl_mem CL_API_CALL
icd_clCreateImage(cl_context              context,
                  cl_mem_flags            flags,
                  const cl_image_format * image_format,
                  const cl_image_desc *   image_desc,
                  void *                  host_ptr,
                  cl_int *                errcode_ret) CL_API_SUFFIX__VERSION_1_2
{
    /** CL_MEM_USE_HOST_PTR and CL_MEM_ALLOC_HOST_PTR are unusable
     * along network, so if detected CL_INVALID_VALUE will
     * returned. In future developments a walking around method can
     * be drafted.
     */
    if( (flags & CL_MEM_USE_HOST_PTR) || (flags & CL_MEM_ALLOC_HOST_PTR) ){
        if(errcode_ret) *errcode_ret=CL_INVALID_VALUE;
        return NULL;
    }
    cl_mem mem_obj = (cl_mem)malloc(sizeof(struct _cl_mem));
    if(!mem_obj){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    mem_obj->dispatch = &master_dispatch;
    mem_obj->ptr = oclandCreateImage(context->ptr, flags, image_format, image_desc, host_ptr, errcode_ret);
    num_master_mems++;
    master_mems[num_master_mems-1] = mem_obj;
    return mem_obj;
}
SYMB(clCreateImage);

CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED cl_mem CL_API_CALL
icd_clCreateImage2D(cl_context              context ,
                    cl_mem_flags            flags ,
                    const cl_image_format * image_format ,
                    size_t                  image_width ,
                    size_t                  image_height ,
                    size_t                  image_row_pitch ,
                    void *                  host_ptr ,
                    cl_int *                errcode_ret) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED
{
    cl_image_desc image_desc;
    image_desc.image_width       = image_width;
    image_desc.image_height      = image_height;
    image_desc.image_depth       = 1;
    image_desc.image_array_size  = 1;
    image_desc.image_row_pitch   = image_row_pitch;
    image_desc.image_slice_pitch = 0;
    image_desc.num_mip_levels    = 0;
    image_desc.num_samples       = 0;
    image_desc.buffer            = NULL;
    return icd_clCreateImage(context, flags, image_format, &image_desc, host_ptr, errcode_ret);
}
SYMB(clCreateImage2D);

CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED cl_mem CL_API_CALL
icd_clCreateImage3D(cl_context              context,
                    cl_mem_flags            flags,
                    const cl_image_format * image_format,
                    size_t                  image_width,
                    size_t                  image_height ,
                    size_t                  image_depth ,
                    size_t                  image_row_pitch ,
                    size_t                  image_slice_pitch ,
                    void *                  host_ptr ,
                    cl_int *                errcode_ret) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED
{
    cl_image_desc image_desc;
    image_desc.image_width       = image_width;
    image_desc.image_height      = image_height;
    image_desc.image_depth       = image_depth;
    image_desc.image_array_size  = 1;
    image_desc.image_row_pitch   = image_row_pitch;
    image_desc.image_slice_pitch = image_slice_pitch;
    image_desc.num_mip_levels    = 0;
    image_desc.num_samples       = 0;
    image_desc.buffer            = NULL;
    return icd_clCreateImage(context, flags, image_format, &image_desc, host_ptr, errcode_ret);
}
SYMB(clCreateImage3D);

// --------------------------------------------------------------
// Samplers
// --------------------------------------------------------------

CL_API_ENTRY cl_sampler CL_API_CALL
icd_clCreateSampler(cl_context           context ,
                    cl_bool              normalized_coords ,
                    cl_addressing_mode   addressing_mode ,
                    cl_filter_mode       filter_mode ,
                    cl_int *             errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_sampler mem_obj = (cl_sampler)malloc(sizeof(struct _cl_sampler));
    if(!mem_obj){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    mem_obj->dispatch = &master_dispatch;
    mem_obj->ptr = (void*)oclandCreateSampler(context->ptr,normalized_coords,addressing_mode,filter_mode,errcode_ret);
    num_master_mems++;
    master_mems[num_master_mems-1] = (cl_mem)mem_obj;
    return mem_obj;
}
SYMB(clCreateSampler);

CL_API_ENTRY cl_int CL_API_CALL
icd_clRetainSampler(cl_sampler  sampler) CL_API_SUFFIX__VERSION_1_0
{
    return oclandRetainSampler(sampler->ptr);
}
SYMB(clRetainSampler);

CL_API_ENTRY cl_int CL_API_CALL
icd_clReleaseSampler(cl_sampler  sampler) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,j;
    cl_int flag = oclandReleaseSampler(sampler->ptr);
    free(sampler);
    for(i=0;i<num_master_mems;i++){
        if(master_mems[i] == (cl_mem)sampler){
            for(j=i+1;j<num_master_mems;j++)
                master_mems[j-1] = master_mems[j];
            break;
        }
    }
    num_master_mems--;
    return flag;
}
SYMB(clReleaseSampler);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetSamplerInfo(cl_sampler          sampler ,
                     cl_sampler_info     param_name ,
                     size_t              param_value_size ,
                     void *              param_value ,
                     size_t *            param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    cl_int flag = oclandGetSamplerInfo(sampler->ptr,param_name,param_value_size,param_value,param_value_size_ret);
    // If requested data is a context, must be convinently corrected
    if((param_name == CL_SAMPLER_CONTEXT) && param_value){
        cl_context *context = param_value;
        for(i=0;i<num_master_contexts;i++){
            if(master_contexts[i]->ptr == *context){
                *context = (void*) master_contexts[i];
                break;
            }
        }
    }
    return flag;
}
SYMB(clGetSamplerInfo);

// --------------------------------------------------------------
// Programs
// --------------------------------------------------------------

CL_API_ENTRY cl_program CL_API_CALL
icd_clCreateProgramWithSource(cl_context         context ,
                              cl_uint            count ,
                              const char **      strings ,
                              const size_t *     lengths ,
                              cl_int *           errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    if((!count) || (!strings)){
        if(errcode_ret) *errcode_ret=CL_INVALID_VALUE;
        return NULL;
    }
    unsigned int i;
    for(i=0;i<count;i++){
        if(!strings[i]){
            if(errcode_ret) *errcode_ret=CL_INVALID_VALUE;
            return NULL;
        }
    }

    cl_program program = (cl_program)malloc(sizeof(struct _cl_program));
    if(!program){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    program->dispatch = &master_dispatch;
    program->ptr = oclandCreateProgramWithSource(context->ptr,count,strings,lengths,errcode_ret);
    num_master_programs++;
    master_programs[num_master_programs-1] = program;
    return program;
}
SYMB(clCreateProgramWithSource);

CL_API_ENTRY cl_program CL_API_CALL
icd_clCreateProgramWithBinary(cl_context                      context ,
                              cl_uint                         num_devices ,
                              const cl_device_id *            device_list ,
                              const size_t *                  lengths ,
                              const unsigned char **          binaries ,
                              cl_int *                        binary_status ,
                              cl_int *                        errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    if(!num_devices || !device_list || !lengths || !binaries){
        if(errcode_ret) *errcode_ret=CL_INVALID_VALUE;
        return NULL;
    }
    unsigned int i;
    for(i=0;i<num_devices;i++){
        if(!lengths[i] || !binaries[i]){
            if(errcode_ret) *errcode_ret=CL_INVALID_VALUE;
            return NULL;
        }
    }

    cl_program program = (cl_program)malloc(sizeof(struct _cl_program));
    if(!program){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    program->dispatch = &master_dispatch;
    program->ptr = oclandCreateProgramWithBinary(context->ptr,num_devices,device_list,lengths,binaries,binary_status,errcode_ret);
    num_master_programs++;
    master_programs[num_master_programs-1] = program;
    return program;
}
SYMB(clCreateProgramWithBinary);

CL_API_ENTRY cl_int CL_API_CALL
icd_clRetainProgram(cl_program  program) CL_API_SUFFIX__VERSION_1_0
{
    return oclandRetainProgram(program->ptr);
}
SYMB(clRetainProgram);

CL_API_ENTRY cl_int CL_API_CALL
icd_clReleaseProgram(cl_program  program) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,j;
    cl_int flag = oclandReleaseProgram(program->ptr);
    free(program);
    for(i=0;i<num_master_programs;i++){
        if(master_programs[i] == program){
            for(j=i+1;j<num_master_programs;j++)
                master_programs[j-1] = master_programs[j];
            break;
        }
    }
    num_master_programs--;
    return flag;
}
SYMB(clReleaseProgram);

CL_API_ENTRY cl_int CL_API_CALL
icd_clBuildProgram(cl_program            program ,
                   cl_uint               num_devices ,
                   const cl_device_id *  device_list ,
                   const char *          options ,
                   void (CL_CALLBACK *   pfn_notify)(cl_program  program , void *  user_data),
                   void *                user_data) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    /** callbacks can't be implemented trought network, so
     * if you request a callback CL_OUT_OF_RESOURCES will
     * be reported.
     */
    if(pfn_notify || user_data){
        return CL_OUT_OF_RESOURCES;
    }
    if((!pfn_notify  &&  user_data  ) ||
       ( num_devices && !device_list) ||
       (!num_devices &&  device_list) ){
        return CL_INVALID_VALUE;
    }
    // We may correct the list of devices
    cl_device_id devs[num_devices];
    for(i=0;i<num_devices;i++){
        devs[i] = device_list[i]->ptr;
    }
    return oclandBuildProgram(program->ptr,num_devices,devs,options,NULL,NULL);
}
SYMB(clBuildProgram);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetProgramInfo(cl_program          program ,
                     cl_program_info     param_name ,
                     size_t              param_value_size ,
                     void *              param_value ,
                     size_t *            param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,j,n;
    cl_int flag = oclandGetProgramInfo(program->ptr,param_name,param_value_size,param_value,param_value_size_ret);
    // If requested data is a context, must be convinently corrected
    if((param_name == CL_PROGRAM_CONTEXT) && param_value){
        cl_context *context = param_value;
        for(i=0;i<num_master_contexts;i++){
            if(master_contexts[i]->ptr == *context){
                *context = (void*) master_contexts[i];
                break;
            }
        }
    }
    // If requested data is the devices list, must be convinently corrected
    if((param_name == CL_PROGRAM_DEVICES) && param_value){
        n = param_value_size / sizeof(cl_device_id);
        for(i=0;i<n;i++){
            cl_device_id *device = param_value + i*sizeof(cl_device_id);
            for(j=0;j<num_master_devices;j++){
                if(master_devices[j].ptr == *device){
                    *device = (void*) &master_devices[j];
                    break;
                }
            }
        }
    }
    return flag;
}
SYMB(clGetProgramInfo);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetProgramBuildInfo(cl_program             program ,
                          cl_device_id           device ,
                          cl_program_build_info  param_name ,
                          size_t                 param_value_size ,
                          void *                 param_value ,
                          size_t *               param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    return oclandGetProgramBuildInfo(program->ptr,device->ptr,param_name,param_value_size,param_value,param_value_size_ret);
}
SYMB(clGetProgramBuildInfo);

CL_API_ENTRY cl_program CL_API_CALL
icd_clCreateProgramWithBuiltInKernels(cl_context             context ,
                                      cl_uint                num_devices ,
                                      const cl_device_id *   device_list ,
                                      const char *           kernel_names ,
                                      cl_int *               errcode_ret) CL_API_SUFFIX__VERSION_1_2
{
    cl_uint i;
    if(!num_devices || !device_list || !kernel_names){
        if(errcode_ret) *errcode_ret=CL_INVALID_VALUE;
        return NULL;
    }
    // Correct devices list
    cl_device_id devices[num_devices];
    for(i=0;i<num_devices;i++){
        devices[i] = device_list[i]->ptr;
    }

    cl_program program = (cl_program)malloc(sizeof(struct _cl_program));
    if(!program){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    program->dispatch = &master_dispatch;
    program->ptr = oclandCreateProgramWithBuiltInKernels(context->ptr,num_devices,devices,kernel_names,errcode_ret);
    num_master_programs++;
    master_programs[num_master_programs-1] = program;
    return program;
}
SYMB(clCreateProgramWithBuiltInKernels);

CL_API_ENTRY cl_int CL_API_CALL
icd_clCompileProgram(cl_program            program ,
                     cl_uint               num_devices ,
                     const cl_device_id *  device_list ,
                     const char *          options ,
                     cl_uint               num_input_headers ,
                     const cl_program *    input_headers,
                     const char **         header_include_names ,
                     void (CL_CALLBACK *   pfn_notify)(cl_program  program , void *  user_data),
                     void *                user_data) CL_API_SUFFIX__VERSION_1_2
{
    cl_uint i;
    /** callbacks can't be implemented trought network, so
     * if you request a callback CL_OUT_OF_RESOURCES will
     * be reported.
     */
    if(pfn_notify || user_data){
        return CL_OUT_OF_RESOURCES;
    }
    if((!pfn_notify  &&  user_data  ) ||
       ( num_devices && !device_list) ||
       (!num_devices &&  device_list) ||
       (!num_input_headers && ( input_headers ||  header_include_names) ) ||
       ( num_input_headers && (!input_headers || !header_include_names) ) ){
        return CL_INVALID_VALUE;
    }
    // Correct devices list
    cl_device_id *devices = NULL;
    if(num_devices){
        devices = (cl_device_id*)malloc(num_devices*sizeof(cl_device_id));
        if(!devices)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_devices;i++){
            devices[i] = device_list[i]->ptr;
        }
    }
    cl_int flag = oclandCompileProgram(program->ptr,num_devices,devices,options,num_input_headers,input_headers,header_include_names,NULL,NULL);
    free(devices); devices=NULL;
    return flag;
}
SYMB(clCompileProgram);

CL_API_ENTRY cl_program CL_API_CALL
icd_clLinkProgram(cl_context            context ,
              cl_uint               num_devices ,
              const cl_device_id *  device_list ,
              const char *          options ,
              cl_uint               num_input_programs ,
              const cl_program *    input_programs ,
              void (CL_CALLBACK *   pfn_notify)(cl_program  program , void *  user_data),
              void *                user_data ,
              cl_int *              errcode_ret) CL_API_SUFFIX__VERSION_1_2
{
    cl_uint i;
    /** callbacks can't be implemented trought network, so
     * if you request a callback CL_OUT_OF_RESOURCES will
     * be reported.
     */
    if(pfn_notify || user_data){
        if(errcode_ret) *errcode_ret=CL_OUT_OF_RESOURCES;
        return NULL;
    }
    if((!pfn_notify  &&  user_data  ) ||
       ( num_devices && !device_list) ||
       (!num_devices &&  device_list) ||
       (!num_input_programs || !input_programs) ){
        if(errcode_ret) *errcode_ret=CL_OUT_OF_RESOURCES;
        return NULL;
    }
    // Correct devices list
    cl_device_id *devices = NULL;
    if(num_devices){
        devices = (cl_device_id*)malloc(num_devices*sizeof(cl_device_id));
        if(!devices){
            if(errcode_ret)
                *errcode_ret = CL_OUT_OF_HOST_MEMORY;
            return NULL;
        }
        for(i=0;i<num_devices;i++){
            devices[i] = device_list[i]->ptr;
        }
    }
    // Correct programs list
    cl_program *programs = NULL;
    if(num_input_programs){
        programs = (cl_program*)malloc(num_devices*sizeof(cl_program));
        if(!programs){
            if(errcode_ret)
                *errcode_ret = CL_OUT_OF_HOST_MEMORY;
            return NULL;
        }
        for(i=0;i<num_input_programs;i++){
            programs[i] = input_programs[i]->ptr;
        }
    }

    cl_program program = (cl_program)malloc(sizeof(struct _cl_program));
    if(!program){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    program->dispatch = &master_dispatch;
    program->ptr = oclandLinkProgram(context->ptr,num_devices,devices,options,num_input_programs,programs,NULL,NULL,errcode_ret);
    free(devices); devices=NULL;
    free(programs); programs=NULL;
    num_master_programs++;
    master_programs[num_master_programs-1] = program;
    return program;
}
SYMB(clLinkProgram);

CL_API_ENTRY cl_int CL_API_CALL
icd_clUnloadPlatformCompiler(cl_platform_id  platform) CL_API_SUFFIX__VERSION_1_2
{
    return clUnloadPlatformCompiler(platform->ptr);
}
SYMB(clUnloadPlatformCompiler);

// --------------------------------------------------------------
// Kernels
// --------------------------------------------------------------

CL_API_ENTRY cl_kernel CL_API_CALL
icd_clCreateKernel(cl_program       program ,
                   const char *     kernel_name ,
                   cl_int *         errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_kernel kernel = (cl_kernel)malloc(sizeof(struct _cl_kernel));
    if(!kernel){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    kernel->dispatch = &master_dispatch;
    kernel->ptr = oclandCreateKernel(program->ptr,kernel_name,errcode_ret);
    num_master_kernels++;
    master_kernels[num_master_kernels-1] = kernel;
    return kernel;
}
SYMB(clCreateKernel);

CL_API_ENTRY cl_int CL_API_CALL
icd_clCreateKernelsInProgram(cl_program      program ,
                             cl_uint         num_kernels ,
                             cl_kernel *     kernels ,
                             cl_uint *       num_kernels_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,n;
    if(    ( !kernels && !num_kernels_ret )
        || ( !kernels &&  num_kernels )
        || (  kernels && !num_kernels ) )
        return CL_INVALID_VALUE;
    cl_int flag = oclandCreateKernelsInProgram(program->ptr,num_kernels,kernels,&n);
    if(flag != CL_SUCCESS)
        return flag;
    if(num_kernels_ret)
        *num_kernels_ret = n;
    if(num_kernels){
        n = num_kernels<n?num_kernels:n;
        for(i=0;i<n;i++){
            cl_kernel kernel = (cl_kernel)malloc(sizeof(struct _cl_kernel));
            if(!kernel){
                return CL_OUT_OF_HOST_MEMORY;
            }
            kernel->dispatch = &master_dispatch;
            kernel->ptr      = kernels[i];
            kernels[i]       = kernel;
            num_master_kernels++;
            master_kernels[num_master_kernels-1] = kernel;
        }
    }
    return CL_SUCCESS;
}
SYMB(clCreateKernelsInProgram);

CL_API_ENTRY cl_int CL_API_CALL
icd_clRetainKernel(cl_kernel     kernel) CL_API_SUFFIX__VERSION_1_0
{
    return oclandRetainKernel(kernel->ptr);
}
SYMB(clRetainKernel);

CL_API_ENTRY cl_int CL_API_CALL
icd_clReleaseKernel(cl_kernel    kernel) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,j;
    cl_int flag = oclandReleaseKernel(kernel->ptr);
    free(kernel);
    for(i=0;i<num_master_kernels;i++){
        if(master_kernels[i] == kernel){
            for(j=i+1;j<num_master_kernels;j++)
                master_kernels[j-1] = master_kernels[j];
            break;
        }
    }
    num_master_kernels--;
    return flag;
}
SYMB(clReleaseKernel);

CL_API_ENTRY cl_int CL_API_CALL
icd_clSetKernelArg(cl_kernel     kernel ,
                   cl_uint       arg_index ,
                   size_t        arg_size ,
                   const void *  arg_value) CL_API_SUFFIX__VERSION_1_0
{
    /** @warning We need to estudy heuristically if the passed argument
     * is a cl_mem or cl_sampler, this is a problem because cl_mem and
     * cl_sampler, being pointers, have the same size of long int, long
     * unsigned int, double types... (can depends on architecture), so
     * if the value matchs with the pointer address will be wrongly
     * considered as a memory object, modifying the value. To avoid it
     * clGetKernelArgInfo must be called, that is a OpenCL 1.2
     * specification, so can be unavailable in some platforms.
     * In case that clGetKernelArgInfo fails we will consider that is a
     * cl_mem object.
     * @note Due to the need of calling clGetKernelArgInfo in some cases
     * does this function relatively slow, and is strongly recommended
     * try to don't call if is not necessary.
     */
    if(arg_size == sizeof(cl_mem)){
        cl_uint i;
        // Can be a cl_mem object
        cl_mem mem_obj = * (cl_mem*)(arg_value);
        for(i=0;i<num_master_mems;i++){
            if(master_mems[i] == mem_obj){
                cl_kernel_arg_address_qualifier arg_address = CL_KERNEL_ARG_ADDRESS_GLOBAL;
                oclandGetKernelArgInfo(kernel->ptr,arg_index,
                                       CL_KERNEL_ARG_ADDRESS_QUALIFIER,
                                       sizeof(cl_kernel_arg_address_qualifier),&arg_address, NULL);
                if(arg_address == CL_KERNEL_ARG_ADDRESS_GLOBAL ){
                    return oclandSetKernelArg(kernel->ptr,arg_index,arg_size,&(mem_obj->ptr));
                }
            }
        }
    }
    return oclandSetKernelArg(kernel->ptr,arg_index,arg_size,arg_value);
}
SYMB(clSetKernelArg);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetKernelInfo(cl_kernel        kernel ,
                    cl_kernel_info   param_name ,
                    size_t           param_value_size ,
                    void *           param_value ,
                    size_t *         param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    cl_int flag = oclandGetKernelInfo(kernel->ptr,param_name,param_value_size,param_value,param_value_size_ret);
    // If requested data is a context, must be convinently corrected
    if((param_name == CL_KERNEL_CONTEXT) && param_value){
        cl_context *context = param_value;
        for(i=0;i<num_master_contexts;i++){
            if(master_contexts[i]->ptr == *context){
                *context = (void*) master_contexts[i];
                break;
            }
        }
    }
    // If requested data is a program, must be convinently corrected
    if((param_name == CL_KERNEL_PROGRAM) && param_value){
        cl_program *program = param_value;
        for(i=0;i<num_master_programs;i++){
            if(master_programs[i]->ptr == *program){
                *program = (void*) master_programs[i];
                break;
            }
        }
    }
    return flag;
}
SYMB(clGetKernelInfo);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetKernelWorkGroupInfo(cl_kernel                   kernel ,
                             cl_device_id                device ,
                             cl_kernel_work_group_info   param_name ,
                             size_t                      param_value_size ,
                             void *                      param_value ,
                             size_t *                    param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    return oclandGetKernelWorkGroupInfo(kernel->ptr,device->ptr,param_name,param_value_size,param_value,param_value_size_ret);
}
SYMB(clGetKernelWorkGroupInfo);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetKernelArgInfo(cl_kernel        kernel ,
                       cl_uint          arg_indx ,
                       cl_kernel_arg_info   param_name ,
                       size_t           param_value_size ,
                       void *           param_value ,
                       size_t *         param_value_size_ret) CL_API_SUFFIX__VERSION_1_2
{
    return oclandGetKernelArgInfo(kernel->ptr,arg_indx,param_name,param_value_size,param_value,param_value_size_ret);
}
SYMB(clGetKernelArgInfo);

// --------------------------------------------------------------
// Events
// --------------------------------------------------------------

CL_API_ENTRY cl_int CL_API_CALL
icd_clWaitForEvents(cl_uint              num_events ,
                    const cl_event *     event_list) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    if(!num_events || !event_list)
        return CL_INVALID_VALUE;

    cl_event events[num_events];
    for(i=0;i<num_events;i++){
        events[i] = event_list[i]->ptr;
    }
    return oclandWaitForEvents(num_events,events);
}
SYMB(clWaitForEvents);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetEventInfo(cl_event          event ,
                   cl_event_info     param_name ,
                   size_t            param_value_size ,
                   void *            param_value ,
                   size_t *          param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    cl_int flag = oclandGetEventInfo(event->ptr,param_name,param_value_size,param_value,param_value_size_ret);
    // If requested data is a command queue, must be convinently corrected
    if((param_name == CL_EVENT_COMMAND_QUEUE) && param_value){
        cl_command_queue *queue = param_value;
        for(i=0;i<num_master_queues;i++){
            if(master_queues[i]->ptr == *queue){
                *queue = (void*) master_queues[i];
                break;
            }
        }
    }
    return flag;
}
SYMB(clGetEventInfo);

CL_API_ENTRY cl_int CL_API_CALL
icd_clRetainEvent(cl_event  event) CL_API_SUFFIX__VERSION_1_0
{
    return oclandRetainEvent(event->ptr);
}
SYMB(clRetainEvent);

CL_API_ENTRY cl_int CL_API_CALL
icd_clReleaseEvent(cl_event  event) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i,j;
    cl_int flag = oclandReleaseEvent(event->ptr);
    free(event);
    for(i=0;i<num_master_events;i++){
        if(master_events[i] == event){
            for(j=i+1;j<num_master_events;j++)
                master_events[j-1] = master_events[j];
            break;
        }
    }
    num_master_events--;
    return flag;
}
SYMB(clReleaseEvent);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetEventProfilingInfo(cl_event             event ,
                            cl_profiling_info    param_name ,
                            size_t               param_value_size ,
                            void *               param_value ,
                            size_t *             param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
    return oclandGetEventProfilingInfo(event->ptr,param_name,param_value_size,param_value,param_value_size_ret);
}
SYMB(clGetEventProfilingInfo);

CL_API_ENTRY cl_event CL_API_CALL
icd_clCreateUserEvent(cl_context     context ,
                      cl_int *       errcode_ret) CL_API_SUFFIX__VERSION_1_1
{
    cl_event event = (cl_event)malloc(sizeof(struct _cl_event));
    if(!event){
        if(errcode_ret)
            *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    event->dispatch = &master_dispatch;
    event->ptr = oclandCreateUserEvent(context->ptr,errcode_ret);
    num_master_events++;
    master_events[num_master_events-1] = event;
    return event;
}
SYMB(clCreateUserEvent);

CL_API_ENTRY cl_int CL_API_CALL
icd_clSetUserEventStatus(cl_event    event ,
                         cl_int      execution_status) CL_API_SUFFIX__VERSION_1_1
{
    return oclandSetUserEventStatus(event->ptr,execution_status);
}
SYMB(clSetUserEventStatus);

CL_API_ENTRY cl_int CL_API_CALL
icd_clSetEventCallback(cl_event     event ,
                       cl_int       command_exec_callback_type ,
                       void (CL_CALLBACK *  pfn_notify)(cl_event, cl_int, void *),
                       void *       user_data) CL_API_SUFFIX__VERSION_1_1
{
    if(!pfn_notify || (command_exec_callback_type != CL_COMPLETE))
        return CL_INVALID_VALUE;
    /** Callbacks can't be registered in ocland due
     * to the implicit network interface, so this
     * operation may fail ever.
     */
    return CL_INVALID_EVENT;
}
SYMB(clSetEventCallback);

// --------------------------------------------------------------
// Enqueues
// --------------------------------------------------------------

CL_API_ENTRY cl_int CL_API_CALL
icd_clFlush(cl_command_queue  command_queue) CL_API_SUFFIX__VERSION_1_0
{
    return oclandFlush(command_queue->ptr);
}
SYMB(clFlush);

CL_API_ENTRY cl_int CL_API_CALL
icd_clFinish(cl_command_queue  command_queue) CL_API_SUFFIX__VERSION_1_0
{
    return oclandFinish(command_queue->ptr);
}
SYMB(clFinish);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueReadBuffer(cl_command_queue     command_queue ,
                        cl_mem               buffer ,
                        cl_bool              blocking_read ,
                        size_t               offset ,
                        size_t               cb ,
                        void *               ptr ,
                        cl_uint              num_events_in_wait_list ,
                        const cl_event *     event_wait_list ,
                        cl_event *           event) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    if(!ptr)
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueReadBuffer(command_queue->ptr,buffer->ptr,
                                          blocking_read,offset,cb,ptr,
                                          num_events_in_wait_list,events_wait,
                                          event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueReadBuffer);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueWriteBuffer(cl_command_queue    command_queue ,
                         cl_mem              buffer ,
                         cl_bool             blocking_write ,
                         size_t              offset ,
                         size_t              cb ,
                         const void *        ptr ,
                         cl_uint             num_events_in_wait_list ,
                         const cl_event *    event_wait_list ,
                         cl_event *          event) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    if(!ptr)
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueWriteBuffer(command_queue->ptr,buffer->ptr,
                                           blocking_write,offset,cb,ptr,
                                           num_events_in_wait_list,events_wait,event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueWriteBuffer);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueCopyBuffer(cl_command_queue     command_queue ,
                        cl_mem               src_buffer ,
                        cl_mem               dst_buffer ,
                        size_t               src_offset ,
                        size_t               dst_offset ,
                        size_t               cb ,
                        cl_uint              num_events_in_wait_list ,
                        const cl_event *     event_wait_list ,
                        cl_event *           event) CL_API_SUFFIX__VERSION_1_0
{
    cl_uint i;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueCopyBuffer(command_queue->ptr,
                                          src_buffer->ptr,dst_buffer->ptr,
                                          src_offset,dst_offset,cb,
                                          num_events_in_wait_list,events_wait,event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueCopyBuffer);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueReadImage(cl_command_queue      command_queue ,
                       cl_mem                image ,
                       cl_bool               blocking_read ,
                       const size_t *        origin ,
                       const size_t *        region ,
                       size_t                row_pitch ,
                       size_t                slice_pitch ,
                       void *                ptr ,
                       cl_uint               num_events_in_wait_list ,
                       const cl_event *      event_wait_list ,
                       cl_event *            event) CL_API_SUFFIX__VERSION_1_0
{
    if(   (!ptr)
       || (!origin)
       || (!region))
        return CL_INVALID_VALUE;
    // Correct some values if not provided
    if(!row_pitch)
        row_pitch   = region[0];
    if(!slice_pitch)
        slice_pitch = region[1]*row_pitch;
    if(   (!region[0]) || (!region[1]) || (!region[2])
       || (row_pitch   < region[0])
       || (slice_pitch < region[1]*row_pitch)
       || (slice_pitch % row_pitch))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueReadImage(command_queue->ptr,image->ptr,
                                         blocking_read,origin,region,
                                         row_pitch,slice_pitch,ptr,
                                         num_events_in_wait_list,events_wait,
                                         event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueReadImage);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueWriteImage(cl_command_queue     command_queue ,
                        cl_mem               image ,
                        cl_bool              blocking_write ,
                        const size_t *       origin ,
                        const size_t *       region ,
                        size_t               row_pitch ,
                        size_t               slice_pitch ,
                        const void *         ptr ,
                        cl_uint              num_events_in_wait_list ,
                        const cl_event *     event_wait_list ,
                        cl_event *           event) CL_API_SUFFIX__VERSION_1_0
{
    // Test minimum data properties
    if(   (!ptr)
       || (!origin)
       || (!region))
        return CL_INVALID_VALUE;
    // Correct some values if not provided
    if(!row_pitch)
        row_pitch   = region[0];
    if(!slice_pitch)
        slice_pitch = region[1]*row_pitch;
    if(   (!region[0]) || (!region[1]) || (!region[2])
       || (row_pitch   < region[0])
       || (slice_pitch < region[1]*row_pitch)
       || (slice_pitch % row_pitch))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueWriteImage(command_queue->ptr,image->ptr,
                                          blocking_write,origin,region,
                                          row_pitch,slice_pitch,ptr,
                                          num_events_in_wait_list,events_wait,
                                          event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueWriteImage);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueCopyImage(cl_command_queue      command_queue ,
                       cl_mem                src_image ,
                       cl_mem                dst_image ,
                       const size_t *        src_origin ,
                       const size_t *        dst_origin ,
                       const size_t *        region ,
                       cl_uint               num_events_in_wait_list ,
                       const cl_event *      event_wait_list ,
                       cl_event *            event) CL_API_SUFFIX__VERSION_1_0
{
    // Test minimum data properties
    if(   (!src_origin)
       || (!dst_origin)
       || (!region))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueCopyImage(command_queue->ptr,
                                         src_image->ptr,dst_image->ptr,
                                         src_origin,dst_origin,region,
                                         num_events_in_wait_list,events_wait,
                                         event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueCopyImage);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueCopyImageToBuffer(cl_command_queue  command_queue ,
                               cl_mem            src_image ,
                               cl_mem            dst_buffer ,
                               const size_t *    src_origin ,
                               const size_t *    region ,
                               size_t            dst_offset ,
                               cl_uint           num_events_in_wait_list ,
                               const cl_event *  event_wait_list ,
                               cl_event *        event) CL_API_SUFFIX__VERSION_1_0
{
    if(   (!src_origin)
       || (!region))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueCopyImageToBuffer(command_queue->ptr,
                                                 src_image->ptr,dst_buffer->ptr,
                                                 src_origin,region,dst_offset,
                                                 num_events_in_wait_list,events_wait,
                                                 event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueCopyImageToBuffer);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueCopyBufferToImage(cl_command_queue  command_queue ,
                               cl_mem            src_buffer ,
                               cl_mem            dst_image ,
                               size_t            src_offset ,
                               const size_t *    dst_origin ,
                               const size_t *    region ,
                               cl_uint           num_events_in_wait_list ,
                               const cl_event *  event_wait_list ,
                               cl_event *        event) CL_API_SUFFIX__VERSION_1_0
{
    if(   (!dst_origin)
       || (!region))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueCopyBufferToImage(command_queue->ptr,
                                                 src_buffer->ptr,dst_image->ptr,
                                                 src_offset,dst_origin,region,
                                                 num_events_in_wait_list,events_wait,
                                                 event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueCopyBufferToImage);

CL_API_ENTRY void * CL_API_CALL
icd_clEnqueueMapBuffer(cl_command_queue  command_queue ,
                       cl_mem            buffer ,
                       cl_bool           blocking_map ,
                       cl_map_flags      map_flags ,
                       size_t            offset ,
                       size_t            cb ,
                       cl_uint           num_events_in_wait_list ,
                       const cl_event *  event_wait_list ,
                       cl_event *        event ,
                       cl_int *          errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    /** ocland doesn't allow mapping memory objects due to the imposibility
     * to have the host pointer and the memory object in the same space.
     */
    *errcode_ret = CL_MAP_FAILURE;
    return NULL;
}
SYMB(clEnqueueMapBuffer);

CL_API_ENTRY void * CL_API_CALL
icd_clEnqueueMapImage(cl_command_queue   command_queue ,
                      cl_mem             image ,
                      cl_bool            blocking_map ,
                      cl_map_flags       map_flags ,
                      const size_t *     origin ,
                      const size_t *     region ,
                      size_t *           image_row_pitch ,
                      size_t *           image_slice_pitch ,
                      cl_uint            num_events_in_wait_list ,
                      const cl_event *   event_wait_list ,
                      cl_event *         event ,
                      cl_int *           errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
    /** ocland doesn't allow mapping memory objects due to the imposibility
     * to have the host pointer and the memory object in the same space.
     */
    *errcode_ret = CL_MAP_FAILURE;
    return NULL;
}
SYMB(clEnqueueMapImage);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueUnmapMemObject(cl_command_queue  command_queue ,
                            cl_mem            memobj ,
                            void *            mapped_ptr ,
                            cl_uint           num_events_in_wait_list ,
                            const cl_event *   event_wait_list ,
                            cl_event *         event) CL_API_SUFFIX__VERSION_1_0
{
    /// In ocland memopry cannot be mapped, so never can be unmapped
    return CL_INVALID_VALUE;
}
SYMB(clEnqueueUnmapMemObject);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueNDRangeKernel(cl_command_queue  command_queue ,
                           cl_kernel         kernel ,
                           cl_uint           work_dim ,
                           const size_t *    global_work_offset ,
                           const size_t *    global_work_size ,
                           const size_t *    local_work_size ,
                           cl_uint           num_events_in_wait_list ,
                           const cl_event *  event_wait_list ,
                           cl_event *        event) CL_API_SUFFIX__VERSION_1_0
{
    if((work_dim < 1) || (work_dim > 3))
        return CL_INVALID_WORK_DIMENSION;
    if(!global_work_size)
        return CL_INVALID_WORK_GROUP_SIZE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueNDRangeKernel(command_queue->ptr,kernel->ptr,
                                             work_dim,global_work_offset,
                                             global_work_size,local_work_size,
                                             num_events_in_wait_list,events_wait,
                                             event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueNDRangeKernel);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueTask(cl_command_queue   command_queue ,
                  cl_kernel          kernel ,
                  cl_uint            num_events_in_wait_list ,
                  const cl_event *   event_wait_list ,
                  cl_event *         event) CL_API_SUFFIX__VERSION_1_0
{
    /** Following OpenCL specification, this method is equivalent
     * to call clEnqueueNDRangeKernel with: \n
     * work_dim = 1
     * global_work_offset = NULL
     * global_work_size[0] = 1
     * local_work_size[0] = 1
     */
    cl_uint work_dim = 1;
    size_t *global_work_offset=NULL;
    size_t global_work_size=1;
    size_t local_work_size=1;
    return icd_clEnqueueNDRangeKernel(command_queue, kernel,work_dim,
                                      global_work_offset,&global_work_size,
                                      &local_work_size,
                                      num_events_in_wait_list,event_wait_list,
                                      event);
}
SYMB(clEnqueueTask);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueNativeKernel(cl_command_queue   command_queue ,
                          void (CL_CALLBACK *user_func)(void *),
                          void *             args ,
                          size_t             cb_args ,
                          cl_uint            num_mem_objects ,
                          const cl_mem *     mem_list ,
                          const void **      args_mem_loc ,
                          cl_uint            num_events_in_wait_list ,
                          const cl_event *   event_wait_list ,
                          cl_event *         event) CL_API_SUFFIX__VERSION_1_0
{
    /// Native kernels cannot be supported by remote applications
    return CL_INVALID_OPERATION;
}
SYMB(clEnqueueNativeKernel);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueReadBufferRect(cl_command_queue     command_queue ,
                            cl_mem               buffer ,
                            cl_bool              blocking_read ,
                            const size_t *       buffer_origin ,
                            const size_t *       host_origin ,
                            const size_t *       region ,
                            size_t               buffer_row_pitch ,
                            size_t               buffer_slice_pitch ,
                            size_t               host_row_pitch ,
                            size_t               host_slice_pitch ,
                            void *               ptr ,
                            cl_uint              num_events_in_wait_list ,
                            const cl_event *     event_wait_list ,
                            cl_event *           event) CL_API_SUFFIX__VERSION_1_1
{
    // Test minimum data properties
    if(   (!ptr)
       || (!buffer_origin)
       || (!host_origin)
       || (!region))
        return CL_INVALID_VALUE;
    // Correct some values if not provided
    if(!buffer_row_pitch)
        buffer_row_pitch   = region[0];
    if(!host_row_pitch)
        host_row_pitch     = region[0];
    if(!buffer_slice_pitch)
        buffer_slice_pitch = region[1]*buffer_row_pitch;
    if(!host_slice_pitch)
        host_slice_pitch   = region[1]*host_row_pitch;
    if(   (!region[0]) || (!region[1]) || (!region[2])
       || (buffer_row_pitch   < region[0])
       || (host_row_pitch     < region[0])
       || (buffer_slice_pitch < region[1]*buffer_row_pitch)
       || (host_slice_pitch   < region[1]*host_row_pitch)
       || (buffer_slice_pitch % buffer_row_pitch)
       || (host_slice_pitch   % host_row_pitch))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueReadBufferRect(command_queue->ptr,buffer->ptr,blocking_read,
                                              buffer_origin,host_origin,region,
                                              buffer_row_pitch,buffer_slice_pitch,
                                              host_row_pitch,host_slice_pitch,ptr,
                                              num_events_in_wait_list,events_wait,
                                              event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueReadBufferRect);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueWriteBufferRect(cl_command_queue     command_queue ,
                             cl_mem               buffer ,
                             cl_bool              blocking_write ,
                             const size_t *       buffer_origin ,
                             const size_t *       host_origin ,
                             const size_t *       region ,
                             size_t               buffer_row_pitch ,
                             size_t               buffer_slice_pitch ,
                             size_t               host_row_pitch ,
                             size_t               host_slice_pitch ,
                             const void *         ptr ,
                             cl_uint              num_events_in_wait_list ,
                             const cl_event *     event_wait_list ,
                             cl_event *           event) CL_API_SUFFIX__VERSION_1_1
{
    if(   (!ptr)
       || (!buffer_origin)
       || (!host_origin)
       || (!region))
        return CL_INVALID_VALUE;
    // Correct some values if not provided
    if(!buffer_row_pitch)
        buffer_row_pitch   = region[0];
    if(!host_row_pitch)
        host_row_pitch     = region[0];
    if(!buffer_slice_pitch)
        buffer_slice_pitch = region[1]*buffer_row_pitch;
    if(!host_slice_pitch)
        host_slice_pitch   = region[1]*host_row_pitch;
    if(   (!region[0]) || (!region[1]) || (!region[2])
       || (buffer_row_pitch   < region[0])
       || (host_row_pitch     < region[0])
       || (buffer_slice_pitch < region[1]*buffer_row_pitch)
       || (host_slice_pitch   < region[1]*host_row_pitch)
       || (buffer_slice_pitch % buffer_row_pitch)
       || (host_slice_pitch   % host_row_pitch))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueWriteBufferRect(command_queue->ptr,buffer->ptr,blocking_write,
                                               buffer_origin,host_origin,region,
                                               buffer_row_pitch,buffer_slice_pitch,
                                               host_row_pitch,host_slice_pitch,ptr,
                                               num_events_in_wait_list,events_wait,
                                               event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueWriteBufferRect);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueCopyBufferRect(cl_command_queue     command_queue ,
                            cl_mem               src_buffer ,
                            cl_mem               dst_buffer ,
                            const size_t *       src_origin ,
                            const size_t *       dst_origin ,
                            const size_t *       region ,
                            size_t               src_row_pitch ,
                            size_t               src_slice_pitch ,
                            size_t               dst_row_pitch ,
                            size_t               dst_slice_pitch ,
                            cl_uint              num_events_in_wait_list ,
                            const cl_event *     event_wait_list ,
                            cl_event *           event) CL_API_SUFFIX__VERSION_1_1
{
    if(   (!src_origin)
       || (!dst_origin)
       || (!region))
        return CL_INVALID_VALUE;
    // Correct some values if not provided
    if(!src_row_pitch)
        src_row_pitch   = region[0];
    if(!dst_row_pitch)
        dst_row_pitch   = region[0];
    if(!src_slice_pitch)
        src_slice_pitch = region[1]*src_row_pitch;
    if(!dst_slice_pitch)
        dst_slice_pitch = region[1]*dst_row_pitch;
    if(   (!region[0]) || (!region[1]) || (!region[2])
       || (src_row_pitch   < region[0])
       || (dst_row_pitch   < region[0])
       || (src_slice_pitch < region[1]*src_row_pitch)
       || (dst_slice_pitch < region[1]*dst_row_pitch)
       || (src_slice_pitch % src_row_pitch)
       || (dst_slice_pitch % dst_row_pitch))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueCopyBufferRect(command_queue->ptr,src_buffer->ptr,dst_buffer->ptr,
                                              src_origin,dst_origin,region,
                                              src_row_pitch,src_slice_pitch,
                                              dst_row_pitch,dst_slice_pitch,
                                              num_events_in_wait_list,events_wait,
                                              event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueCopyBufferRect);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueFillBuffer(cl_command_queue    command_queue ,
                        cl_mem              buffer ,
                        const void *        pattern ,
                        size_t              pattern_size ,
                        size_t              offset ,
                        size_t              cb ,
                        cl_uint             num_events_in_wait_list ,
                        const cl_event *    event_wait_list ,
                        cl_event *          event) CL_API_SUFFIX__VERSION_1_2
{
    if((!pattern) || (!pattern_size))
        return CL_INVALID_VALUE;
    if((offset % pattern_size) || (cb % pattern_size))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueFillBuffer(command_queue->ptr,buffer->ptr,
                                          pattern,pattern_size,offset,cb,
                                          num_events_in_wait_list,events_wait,
                                          event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueFillBuffer);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueFillImage(cl_command_queue    command_queue ,
                       cl_mem              image ,
                       const void *        fill_color ,
                       const size_t *      origin ,
                       const size_t *      region ,
                       cl_uint             num_events_in_wait_list ,
                       const cl_event *    event_wait_list ,
                       cl_event *          event) CL_API_SUFFIX__VERSION_1_2
{
    // To use this method before we may get the size of fill_color
    size_t fill_color_size = 4*sizeof(float);
    cl_image_format image_format;
    cl_int flag = clGetImageInfo(image, CL_IMAGE_FORMAT, sizeof(cl_image_format), &image_format, NULL);
    if(flag != CL_SUCCESS)
        return CL_INVALID_MEM_OBJECT;
    if(    image_format.image_channel_data_type == CL_SIGNED_INT8
        || image_format.image_channel_data_type == CL_SIGNED_INT16
        || image_format.image_channel_data_type == CL_SIGNED_INT32 ){
            fill_color_size = 4*sizeof(int);
    }
    if(    image_format.image_channel_data_type == CL_UNSIGNED_INT8
        || image_format.image_channel_data_type == CL_UNSIGNED_INT16
        || image_format.image_channel_data_type == CL_UNSIGNED_INT32 ){
            fill_color_size = 4*sizeof(unsigned int);
    }
    // Test minimum data properties
    if(   (!fill_color)
       || (!origin)
       || (!region))
        return CL_INVALID_VALUE;
    if(   (!region[0]) || (!region[1]) || (!region[2]) )
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    flag = oclandEnqueueFillImage(command_queue->ptr,image->ptr,
                                  fill_color_size,fill_color,origin,region,
                                  num_events_in_wait_list,events_wait,
                                  event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueFillImage);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueMigrateMemObjects(cl_command_queue        command_queue ,
                               cl_uint                 num_mem_objects ,
                               const cl_mem *          mem_objects ,
                               cl_mem_migration_flags  flags ,
                               cl_uint                 num_events_in_wait_list ,
                               const cl_event *        event_wait_list ,
                               cl_event *              event) CL_API_SUFFIX__VERSION_1_2
{
    // Test for valid flags
    if(    (!flags)
        || (    (flags != CL_MIGRATE_MEM_OBJECT_HOST)
             && (flags != CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED)))
        return CL_INVALID_VALUE;
    // Test for other invalid values
    if(    (!num_mem_objects)
        || (!mem_objects))
        return CL_INVALID_VALUE;
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct memory objects
    cl_uint i;
    cl_mem *mems = NULL;
    if(num_mem_objects){
        mems = (cl_mem*)malloc(num_mem_objects*sizeof(cl_mem));
        if(!mems)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_mem_objects;i++)
            mems[i] = mem_objects[i]->ptr;
    }
    // Correct input events
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueMigrateMemObjects(command_queue->ptr,
                                                 num_mem_objects,mems,flags,
                                                 num_events_in_wait_list,events_wait,
                                                 event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueMigrateMemObjects);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueMarkerWithWaitList(cl_command_queue  command_queue ,
                                cl_uint            num_events_in_wait_list ,
                                const cl_event *   event_wait_list ,
                                cl_event *         event) CL_API_SUFFIX__VERSION_1_2
{
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueMarkerWithWaitList(command_queue->ptr,
                                                  num_events_in_wait_list,events_wait,
                                                  event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueMarkerWithWaitList);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueBarrierWithWaitList(cl_command_queue  command_queue ,
                                 cl_uint            num_events_in_wait_list ,
                                 const cl_event *   event_wait_list ,
                                 cl_event *         event) CL_API_SUFFIX__VERSION_1_2
{
    if(    ( num_events_in_wait_list && !event_wait_list)
        || (!num_events_in_wait_list &&  event_wait_list))
        return CL_INVALID_EVENT_WAIT_LIST;
    // Correct input events
    cl_uint i;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait)
            return CL_OUT_OF_HOST_MEMORY;
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    cl_int flag = oclandEnqueueBarrierWithWaitList(command_queue->ptr,
                                                   num_events_in_wait_list,events_wait,
                                                   event);
    if(flag != CL_SUCCESS)
        return flag;
    free(events_wait); events_wait=NULL;
    // Correct output event
    if(event){
        cl_event e = (cl_event)malloc(sizeof(struct _cl_event));
        if(!e){
            return CL_OUT_OF_HOST_MEMORY;
        }
        e->dispatch = &master_dispatch;
        e->ptr = *event;
        *event = e;
        num_master_events++;
        master_events[num_master_events-1] = e;
    }
    return CL_SUCCESS;
}
SYMB(clEnqueueBarrierWithWaitList);

CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED cl_int CL_API_CALL
icd_clEnqueueMarker(cl_command_queue    command_queue ,
                    cl_event *          event) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED
{
    return icd_clEnqueueMarkerWithWaitList(command_queue, 0, NULL, event);
}
SYMB(clEnqueueMarker);

CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED cl_int CL_API_CALL
icd_clEnqueueWaitForEvents(cl_command_queue command_queue ,
                           cl_uint          num_events ,
                           const cl_event * event_list ) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED
{
    return icd_clWaitForEvents(num_events, event_list);
}
SYMB(clEnqueueWaitForEvents);

CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED cl_int CL_API_CALL
icd_clEnqueueBarrier(cl_command_queue command_queue ) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED
{
    return icd_clEnqueueBarrierWithWaitList(command_queue, 0, NULL, NULL);
}
SYMB(clEnqueueBarrier);

// --------------------------------------------------------------
// OpenGL
// --------------------------------------------------------------

CL_API_ENTRY cl_mem CL_API_CALL
icd_clCreateFromGLBuffer(cl_context     context ,
                         cl_mem_flags   flags ,
                         cl_GLuint      bufobj ,
                         int *          errcode_ret ) CL_API_SUFFIX__VERSION_1_0
{
    /** GL objects generated in host are not valid for
     * the server, so this operation can't be executed.
     */
    *errcode_ret = CL_INVALID_GL_OBJECT;
    return NULL;
}
SYMB(clCreateFromGLBuffer);

CL_API_ENTRY cl_mem CL_API_CALL
icd_clCreateFromGLTexture(cl_context      context ,
                          cl_mem_flags    flags ,
                          cl_GLenum       target ,
                          cl_GLint        miplevel ,
                          cl_GLuint       texture ,
                          cl_int *        errcode_ret ) CL_API_SUFFIX__VERSION_1_2
{
    /** GL objects generated in host are not valid for
     * the server, so this operation can't be executed.
     */
    *errcode_ret = CL_INVALID_GL_OBJECT;
    return NULL;
}
SYMB(clCreateFromGLTexture);

CL_API_ENTRY cl_mem CL_API_CALL
icd_clCreateFromGLRenderbuffer(cl_context   context ,
                               cl_mem_flags flags ,
                               cl_GLuint    renderbuffer ,
                               cl_int *     errcode_ret ) CL_API_SUFFIX__VERSION_1_0
{
    /** GL objects generated in host are not valid for
     * the server, so this operation can't be executed.
     */
    *errcode_ret = CL_INVALID_GL_OBJECT;
    return NULL;
}
SYMB(clCreateFromGLRenderbuffer);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetGLObjectInfo(cl_mem                memobj ,
                      cl_gl_object_type *   gl_object_type ,
                      cl_GLuint *           gl_object_name ) CL_API_SUFFIX__VERSION_1_0
{
    /** If have not been possible to generate GL memory
     * objects, is impossible that the memory object is
     * associated to a GL object.
     */
    return CL_INVALID_GL_OBJECT;
}
SYMB(clGetGLObjectInfo);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetGLTextureInfo(cl_mem               memobj ,
                       cl_gl_texture_info   param_name ,
                       size_t               param_value_size ,
                       void *               param_value ,
                       size_t *             param_value_size_ret ) CL_API_SUFFIX__VERSION_1_0
{
    /** If have not been possible to generate GL memory
     * objects, is impossible that the memory object is
     * associated to a GL object.
     */
    return CL_INVALID_GL_OBJECT;
}
SYMB(clGetGLTextureInfo);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueAcquireGLObjects(cl_command_queue      command_queue ,
                              cl_uint               num_objects ,
                              const cl_mem *        mem_objects ,
                              cl_uint               num_events_in_wait_list ,
                              const cl_event *      event_wait_list ,
                              cl_event *            event ) CL_API_SUFFIX__VERSION_1_0
{
    /** If have not been possible to generate GL memory
     * objects, is impossible that the memory object is
     * associated to a GL object.
     */
    return CL_INVALID_GL_OBJECT;
}
SYMB(clEnqueueAcquireGLObjects);

CL_API_ENTRY cl_int CL_API_CALL
icd_clEnqueueReleaseGLObjects(cl_command_queue      command_queue ,
                              cl_uint               num_objects ,
                              const cl_mem *        mem_objects ,
                              cl_uint               num_events_in_wait_list ,
                              const cl_event *      event_wait_list ,
                              cl_event *            event ) CL_API_SUFFIX__VERSION_1_0
{
    /** If have not been possible to generate GL memory
     * objects, is impossible that the memory object is
     * associated to a GL object.
     */
    return CL_INVALID_GL_OBJECT;
}
SYMB(clEnqueueReleaseGLObjects);

CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED cl_mem CL_API_CALL
icd_clCreateFromGLTexture2D(cl_context      context ,
                            cl_mem_flags    flags ,
                            cl_GLenum       target ,
                            cl_GLint        miplevel ,
                            cl_GLuint       texture ,
                            cl_int *        errcode_ret ) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED
{
    /** GL objects generated in host are not valid for
     * the server, so this operation can't be executed.
     */
    *errcode_ret = CL_INVALID_GL_OBJECT;
    return NULL;
}
SYMB(clCreateFromGLTexture2D);

CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED cl_mem CL_API_CALL
icd_clCreateFromGLTexture3D(cl_context      context ,
                            cl_mem_flags    flags ,
                            cl_GLenum       target ,
                            cl_GLint        miplevel ,
                            cl_GLuint       texture ,
                            cl_int *        errcode_ret ) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED
{
    /** GL objects generated in host are not valid for
     * the server, so this operation can't be executed.
     */
    *errcode_ret = CL_INVALID_GL_OBJECT;
    return NULL;
}
SYMB(clCreateFromGLTexture3D);

CL_API_ENTRY cl_int CL_API_CALL
icd_clGetGLContextInfoKHR(const cl_context_properties * properties ,
                          cl_gl_context_info            param_name ,
                          size_t                        param_value_size ,
                          void *                        param_value ,
                          size_t *                      param_value_size_ret ) CL_API_SUFFIX__VERSION_1_0
{
    return CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR;
}
SYMB(clGetGLContextInfoKHR);

// --------------------------------------------------------------
// Extensions, only used at the start of icd_loader
// --------------------------------------------------------------

CL_API_ENTRY void * CL_API_CALL
icd_clGetExtensionFunctionAddress(const char *   func_name) CL_API_SUFFIX__VERSION_1_0
{
    if( func_name != NULL &&  strcmp("clIcdGetPlatformIDsKHR", func_name) == 0 )
        return (void *)__GetPlatformIDs;
    return NULL;
}
SYMB(clGetExtensionFunctionAddress);

CL_API_ENTRY void * CL_API_CALL
icd_clGetExtensionFunctionAddressForPlatform(cl_platform_id platform,
                                             const char *   func_name) CL_API_SUFFIX__VERSION_1_2
{
    return icd_clGetExtensionFunctionAddress(func_name);
}
SYMB(clGetExtensionFunctionAddressForPlatform);

#pragma GCC visibility pop

void dummyFunc(void){}

/*
-1 : clSetPrintfCallback
13  : clSetCommandQueueProperty
91  : clReleaseDeviceEXT
90  : clRetainDeviceEXT
89  : clCreateSubDevicesEXT
-1 : clCreateEventFromGLsyncKHR
*/

struct _cl_icd_dispatch master_dispatch = {
  (void(*)(void))& icd_clGetPlatformIDs,
  (void(*)(void))& icd_clGetPlatformInfo,
  (void(*)(void))& icd_clGetDeviceIDs,
  (void(*)(void))& icd_clGetDeviceInfo,
  (void(*)(void))& icd_clCreateContext,
  (void(*)(void))& icd_clCreateContextFromType,
  (void(*)(void))& icd_clRetainContext,
  (void(*)(void))& icd_clReleaseContext,
  (void(*)(void))& icd_clGetContextInfo,
  (void(*)(void))& icd_clCreateCommandQueue,
  (void(*)(void))& icd_clRetainCommandQueue,
  (void(*)(void))& icd_clReleaseCommandQueue,
  (void(*)(void))& icd_clGetCommandQueueInfo,
  (void(*)(void))& dummyFunc,    // 13,
  (void(*)(void))& icd_clCreateBuffer,
  (void(*)(void))& icd_clCreateImage2D,
  (void(*)(void))& icd_clCreateImage3D,
  (void(*)(void))& icd_clRetainMemObject,
  (void(*)(void))& icd_clReleaseMemObject,
  (void(*)(void))& icd_clGetSupportedImageFormats,
  (void(*)(void))& icd_clGetMemObjectInfo,
  (void(*)(void))& icd_clGetImageInfo,
  (void(*)(void))& icd_clCreateSampler,
  (void(*)(void))& icd_clRetainSampler,
  (void(*)(void))& icd_clReleaseSampler,
  (void(*)(void))& icd_clGetSamplerInfo,
  (void(*)(void))& icd_clCreateProgramWithSource,
  (void(*)(void))& icd_clCreateProgramWithBinary,
  (void(*)(void))& icd_clRetainProgram,
  (void(*)(void))& icd_clReleaseProgram,
  (void(*)(void))& icd_clBuildProgram,
  (void(*)(void))& dummyFunc,    // 31,
  (void(*)(void))& icd_clGetProgramInfo,
  (void(*)(void))& icd_clGetProgramBuildInfo,
  (void(*)(void))& icd_clCreateKernel,
  (void(*)(void))& icd_clCreateKernelsInProgram,
  (void(*)(void))& icd_clRetainKernel,
  (void(*)(void))& icd_clReleaseKernel,
  (void(*)(void))& icd_clSetKernelArg,
  (void(*)(void))& icd_clGetKernelInfo,
  (void(*)(void))& icd_clGetKernelWorkGroupInfo,
  (void(*)(void))& icd_clWaitForEvents,
  (void(*)(void))& icd_clGetEventInfo,
  (void(*)(void))& icd_clRetainEvent,
  (void(*)(void))& icd_clReleaseEvent,
  (void(*)(void))& icd_clGetEventProfilingInfo,
  (void(*)(void))& icd_clFlush,
  (void(*)(void))& icd_clFinish,
  (void(*)(void))& icd_clEnqueueReadBuffer,
  (void(*)(void))& icd_clEnqueueWriteBuffer,
  (void(*)(void))& icd_clEnqueueCopyBuffer,
  (void(*)(void))& icd_clEnqueueReadImage,
  (void(*)(void))& icd_clEnqueueWriteImage,
  (void(*)(void))& icd_clEnqueueCopyImage,
  (void(*)(void))& icd_clEnqueueCopyImageToBuffer,
  (void(*)(void))& icd_clEnqueueCopyBufferToImage,
  (void(*)(void))& icd_clEnqueueMapBuffer,
  (void(*)(void))& icd_clEnqueueMapImage,
  (void(*)(void))& icd_clEnqueueUnmapMemObject,
  (void(*)(void))& icd_clEnqueueNDRangeKernel,
  (void(*)(void))& icd_clEnqueueTask,
  (void(*)(void))& icd_clEnqueueNativeKernel,
  (void(*)(void))& icd_clEnqueueMarker,
  (void(*)(void))& icd_clEnqueueWaitForEvents,
  (void(*)(void))& icd_clEnqueueBarrier,
  (void(*)(void))& dummyFunc,    // 65,
  (void(*)(void))& icd_clCreateFromGLBuffer,
  (void(*)(void))& icd_clCreateFromGLTexture2D,
  (void(*)(void))& icd_clCreateFromGLTexture3D,
  (void(*)(void))& icd_clCreateFromGLRenderbuffer,
  (void(*)(void))& icd_clGetGLObjectInfo,
  (void(*)(void))& icd_clGetGLTextureInfo,
  (void(*)(void))& icd_clEnqueueAcquireGLObjects,
  (void(*)(void))& icd_clEnqueueReleaseGLObjects,
  (void(*)(void))& dummyFunc,    // 74,
  (void(*)(void))& dummyFunc,    // 75,
  (void(*)(void))& dummyFunc,    // 76,
  (void(*)(void))& dummyFunc,    // 77,
  (void(*)(void))& dummyFunc,    // 78,
  (void(*)(void))& dummyFunc,    // 79,
  (void(*)(void))& dummyFunc,    // 80,
  (void(*)(void))& icd_clSetEventCallback,
  (void(*)(void))& icd_clCreateSubBuffer,
  (void(*)(void))& icd_clSetMemObjectDestructorCallback,
  (void(*)(void))& icd_clCreateUserEvent,
  (void(*)(void))& icd_clSetUserEventStatus,
  (void(*)(void))& icd_clEnqueueReadBufferRect,
  (void(*)(void))& icd_clEnqueueWriteBufferRect,
  (void(*)(void))& icd_clEnqueueCopyBufferRect,
  (void(*)(void))& dummyFunc,    // 89,
  (void(*)(void))& dummyFunc,    // 90,
  (void(*)(void))& dummyFunc,    // 91,
  (void(*)(void))& dummyFunc,    // 92,
  (void(*)(void))& icd_clCreateSubDevices,
  (void(*)(void))& icd_clRetainDevice,
  (void(*)(void))& icd_clReleaseDevice,
  (void(*)(void))& icd_clCreateImage,
  (void(*)(void))& icd_clCreateProgramWithBuiltInKernels,
  (void(*)(void))& icd_clCompileProgram,
  (void(*)(void))& icd_clLinkProgram,
  (void(*)(void))& icd_clUnloadPlatformCompiler,
  (void(*)(void))& icd_clGetKernelArgInfo,
  (void(*)(void))& icd_clEnqueueFillBuffer,
  (void(*)(void))& icd_clEnqueueFillImage,
  (void(*)(void))& icd_clEnqueueMigrateMemObjects,
  (void(*)(void))& icd_clEnqueueMarkerWithWaitList,
  (void(*)(void))& icd_clEnqueueBarrierWithWaitList,
  (void(*)(void))& dummyFunc,    // clGetExtensionFunctionAddressForPlatform (don't set it)
  (void(*)(void))& icd_clCreateFromGLTexture,
  (void(*)(void))& dummyFunc,    // 109,
  (void(*)(void))& dummyFunc,    // 110,
  (void(*)(void))& dummyFunc,    // 111,
  (void(*)(void))& dummyFunc,    // 112,
  (void(*)(void))& dummyFunc,    // 113,
  (void(*)(void))& dummyFunc,    // 114,
  (void(*)(void))& dummyFunc,    // 115,
  (void(*)(void))& dummyFunc,    // 116,
  (void(*)(void))& dummyFunc,    // 117,
  (void(*)(void))& dummyFunc,    // 118,
  (void(*)(void))& dummyFunc,    // 119,
  (void(*)(void))& dummyFunc,    // 120,
  (void(*)(void))& dummyFunc,    // 121,
  (void(*)(void))& dummyFunc,    // 122,
  (void(*)(void))& dummyFunc,    // 123,
  (void(*)(void))& dummyFunc,    // 124
};