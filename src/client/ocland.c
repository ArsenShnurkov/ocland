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

#include <errno.h>
#include <signal.h>

#ifdef WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h> //inet_pton
#ifndef inet_pton
    // For now, inet_pton function is missed in MinGW ws2tcpip.h header - see MinGW bug #1641
    INT WSAAPI inet_pton(INT  Family, CONST CHAR * pszAddrString, PVOID pAddrBuf);
#endif
    #define MSG_MORE 0
    #ifndef MSG_WAITALL
        //is undefined on MINGW system headers
        #define MSG_WAITALL 0x8
    #endif
    #ifndef ECONNREFUSED
        // is undefined on MINGW system headera
        #define ECONNREFUSED    107
    #endif
    static int close(int fd) { return closesocket(fd); }
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#include <pthread.h>

#include <ocland/common/dataExchange.h>
#include <ocland/common/dataPack.h>
#include <ocland/client/ocland_icd.h>
#include <ocland/client/ocland.h>
#include <ocland/client/commands_enum.h>
#include <ocland/client/shortcut.h>

#ifndef OCLAND_PORT
    #define OCLAND_PORT 51000u
#endif

#ifndef BUFF_SIZE
    #define BUFF_SIZE 1025u
#endif

#define THREAD_SAFE_EXIT {free(_data); _data=NULL; if(fd>0) close(fd); fd = -1; pthread_exit(NULL); return NULL;}

cl_sampler oclandCreateSampler(cl_context           context ,
                               cl_bool              normalized_coords ,
                               cl_addressing_mode   addressing_mode ,
                               cl_filter_mode       filter_mode ,
                               cl_int *             errcode_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_sampler sampler = NULL;
    unsigned int comm = ocland_clCreateSampler;
    if(errcode_ret) *errcode_ret = CL_SUCCESS;
    // Get the server
    int *sockfd = context->server->socket;
    if(!sockfd){
        if(errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(context->ptr), sizeof(cl_context), MSG_MORE);
    Send(sockfd, &normalized_coords, sizeof(cl_bool), MSG_MORE);
    Send(sockfd, &addressing_mode, sizeof(cl_addressing_mode), MSG_MORE);
    Send(sockfd, &filter_mode, sizeof(cl_filter_mode), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        if(errcode_ret) *errcode_ret = flag;
        return NULL;
    }
    Recv(sockfd, &sampler, sizeof(cl_sampler), MSG_WAITALL);
    addShortcut((void*)sampler, sockfd);
    return sampler;
}

cl_int oclandRetainSampler(cl_sampler sampler)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clRetainSampler;
    // Get the server
    int *sockfd = sampler->socket;
    if(!sockfd){
        return CL_INVALID_SAMPLER;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(sampler->ptr), sizeof(cl_sampler), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_int oclandReleaseSampler(cl_sampler sampler)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clReleaseSampler;
    // Get the server
    int *sockfd = sampler->socket;
    if(!sockfd){
        return CL_INVALID_SAMPLER;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(sampler->ptr), sizeof(cl_sampler), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag == CL_SUCCESS)
        delShortcut(sampler);
    return flag;
}

cl_int oclandGetSamplerInfo(cl_sampler          sampler ,
                            cl_sampler_info     param_name ,
                            size_t              param_value_size ,
                            void *              param_value ,
                            size_t *            param_value_size_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    size_t size_ret=0;
    unsigned int comm = ocland_clGetSamplerInfo;
    if(param_value_size_ret) *param_value_size_ret=0;
    // Get the server
    int *sockfd = sampler->socket;
    if(!sockfd){
        return CL_INVALID_SAMPLER;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(sampler->ptr), sizeof(cl_sampler), MSG_MORE);
    Send(sockfd, &param_name, sizeof(cl_sampler_info), MSG_MORE);
    Send(sockfd, &param_value_size, sizeof(size_t), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    Recv(sockfd, &size_ret, sizeof(size_t), MSG_WAITALL);
    if(param_value_size_ret) *param_value_size_ret = size_ret;
    if(param_value){
        Recv(sockfd, param_value, size_ret, MSG_WAITALL);
    }
    return CL_SUCCESS;
}

cl_program oclandCreateProgramWithSource(cl_context         context ,
                                         cl_uint            count ,
                                         const char **      strings ,
                                         const size_t *     lengths ,
                                         cl_int *           errcode_ret)
{
    unsigned int i;
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_program program = NULL;
    unsigned int comm = ocland_clCreateProgramWithSource;
    if(errcode_ret) *errcode_ret = CL_SUCCESS;
    // Get the server
    int *sockfd = context->server->socket;
    if(!sockfd){
        if(errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    size_t* non_zero_lengths = calloc(count, sizeof(size_t));
    // Two cases handled:
    // 1) lengths is NULL - all strings are null-terminated
    // 2) some lengths are zero - those strings are null-terminated
    if(lengths){
        memcpy(non_zero_lengths, lengths, count * sizeof(size_t));
    }
    for(i=0;i<count;i++){
        if (0 == non_zero_lengths[i]){
            non_zero_lengths[i] = strlen(strings[i]);
        }
    }

    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &context->ptr, sizeof(cl_context), MSG_MORE);
    Send(sockfd, &count, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, non_zero_lengths, count*sizeof(size_t), 0);
    for(i=0;i<count;i++){
        Send(sockfd, strings[i], non_zero_lengths[i], 0);
    }
    free(non_zero_lengths);
    non_zero_lengths = NULL;

    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        if(errcode_ret) *errcode_ret = flag;
        return NULL;
    }
    Recv(sockfd, &program, sizeof(cl_program), MSG_WAITALL);
    addShortcut((void*)program, sockfd);
    return program;
}

cl_program oclandCreateProgramWithBinary(cl_context                      context ,
                                         cl_uint                         num_devices ,
                                         const cl_device_id *            device_list ,
                                         const size_t *                  lengths ,
                                         const unsigned char **          binaries ,
                                         cl_int *                        binary_status ,
                                         cl_int *                        errcode_ret)
{
    unsigned int i;
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_program program = NULL;
    cl_int* status = NULL;
    unsigned int comm = ocland_clCreateProgramWithBinary;
    if(errcode_ret) *errcode_ret = CL_SUCCESS;
    // Get the server
    int *sockfd = context->server->socket;
    if(!sockfd){
        if(errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    // Substitute the local references to the remote ones
    cl_device_id *devices = calloc(num_devices, sizeof(cl_device_id));
    if ((num_devices > 0) && (NULL == devices)){
        if (errcode_ret) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    for(i=0;i<num_devices;i++){
        devices[i] = device_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(context->ptr), sizeof(cl_context), MSG_MORE);
    Send(sockfd, &num_devices, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, devices, num_devices*sizeof(cl_device_id), MSG_MORE);
    Send(sockfd, lengths, num_devices*sizeof(size_t), 0);
    for(i=0;i<num_devices;i++){
        Send(sockfd, binaries[i], lengths[i], 0);
    }
    free(devices); devices = NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        if(errcode_ret) *errcode_ret = flag;
        return NULL;
    }
    Recv(sockfd, &program, sizeof(cl_program), MSG_WAITALL);
    addShortcut((void*)program, sockfd);
    status = (cl_int*)malloc(num_devices*sizeof(cl_int));
    if(!status){
        if(errcode_ret) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    Recv(sockfd, status, num_devices*sizeof(cl_int), MSG_WAITALL);
    if(binary_status)
        memcpy((void*)binary_status, (void*)status, num_devices*sizeof(cl_int));
    return program;
}

cl_int oclandRetainProgram(cl_program  program)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clRetainProgram;
    // Get the server
    int *sockfd = program->socket;
    if(!sockfd){
        return CL_INVALID_PROGRAM;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &program->socket, sizeof(cl_program), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_int oclandReleaseProgram(cl_program  program)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clReleaseProgram;
    // Get the server
    int *sockfd = program->socket;
    if(!sockfd){
        return CL_INVALID_PROGRAM;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(program->ptr), sizeof(cl_program), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag == CL_SUCCESS)
        delShortcut(program->ptr);
    return flag;
}

cl_int oclandBuildProgram(cl_program            program ,
                          cl_uint               num_devices ,
                          const cl_device_id *  device_list ,
                          const char *          options ,
                          void (CL_CALLBACK *   pfn_notify)(cl_program  program , void *  user_data),
                          void *                user_data)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clBuildProgram;
    size_t options_size = (strlen(options) + 1)*sizeof(char);
    // Get the server
    int *sockfd = program->socket;
    if(!sockfd){
        return CL_INVALID_PROGRAM;
    }
    // Substitute the local references to the remote ones
    cl_device_id *devices = calloc(num_devices, sizeof(cl_device_id));
    if ((num_devices > 0) && (NULL == devices)){
        return CL_OUT_OF_HOST_MEMORY;
    }
    for(i=0;i<num_devices;i++){
        devices[i] = device_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(program->ptr), sizeof(cl_program), MSG_MORE);
    Send(sockfd, &num_devices, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, devices, num_devices*sizeof(cl_device_id), MSG_MORE);
    Send(sockfd, &options_size, sizeof(size_t), MSG_MORE);
    Send(sockfd, options, options_size, 0);
    free(devices); devices = NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_int oclandGetProgramInfo(cl_program          program ,
                            cl_program_info     param_name ,
                            size_t              param_value_size ,
                            void *              param_value ,
                            size_t *            param_value_size_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    size_t size_ret=0;
    unsigned int comm = ocland_clGetProgramInfo;
    if(param_value_size_ret) *param_value_size_ret=0;
    // Get the server
    int *sockfd = program->socket;
    if(!sockfd){
        return CL_INVALID_PROGRAM;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(program->ptr), sizeof(cl_program), MSG_MORE);
    Send(sockfd, &param_name, sizeof(cl_program_info), MSG_MORE);
    Send(sockfd, &param_value_size, sizeof(size_t), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    Recv(sockfd, &size_ret, sizeof(size_t), MSG_WAITALL);
    if(param_value_size_ret) *param_value_size_ret = size_ret;
    if(!param_value_size){
        return CL_SUCCESS;
    }
    if(param_name != CL_PROGRAM_BINARIES){
        Recv(sockfd, param_value, size_ret, MSG_WAITALL);
        return CL_SUCCESS;
    }
    for(i = 0; i < program->num_devices; i++){
        if(!program->binary_lengths[i])
            continue;
        Recv(sockfd,
             program->binaries[i],
             program->binary_lengths[i],
             MSG_WAITALL);
        memcpy(((char**)param_value)[i],
               program->binaries[i],
               program->binary_lengths[i]);
    }
    return CL_SUCCESS;
}

cl_int oclandGetProgramBuildInfo(cl_program             program ,
                                 cl_device_id           device ,
                                 cl_program_build_info  param_name ,
                                 size_t                 param_value_size ,
                                 void *                 param_value ,
                                 size_t *               param_value_size_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    size_t size_ret=0;
    unsigned int comm = ocland_clGetProgramBuildInfo;
    if(param_value_size_ret) *param_value_size_ret=0;
    // Get the server
    int *sockfd = program->socket;
    if(!sockfd){
        return CL_INVALID_PROGRAM;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(program->ptr), sizeof(cl_program), MSG_MORE);
    Send(sockfd, &(device->ptr), sizeof(cl_device_id), MSG_MORE);
    Send(sockfd, &param_name, sizeof(cl_program_build_info), MSG_MORE);
    Send(sockfd, &param_value_size, sizeof(size_t), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    Recv(sockfd, &size_ret, sizeof(size_t), MSG_WAITALL);
    if(param_value_size_ret) *param_value_size_ret = size_ret;
    if(param_value){
        Recv(sockfd, param_value, size_ret, MSG_WAITALL);
    }
    return CL_SUCCESS;
}

cl_kernel oclandCreateKernel(cl_program       program ,
                             const char *     kernel_name ,
                             cl_int *         errcode_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_kernel kernel = NULL;
    unsigned int comm = ocland_clCreateKernel;
    size_t kernel_name_size = (strlen(kernel_name)+1)*sizeof(char);
    if(errcode_ret) *errcode_ret = CL_SUCCESS;
    // Get the server
    int *sockfd = program->socket;
    if(!sockfd){
        if(errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(program->ptr), sizeof(cl_program), MSG_MORE);
    Send(sockfd, &kernel_name_size, sizeof(size_t), MSG_MORE);
    Send(sockfd, kernel_name, kernel_name_size, 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        if(errcode_ret) *errcode_ret = flag;
        return NULL;
    }
    Recv(sockfd, &kernel, sizeof(cl_kernel), MSG_WAITALL);
    addShortcut((void*)kernel, sockfd);
    return kernel;
}

cl_int oclandCreateKernelsInProgram(cl_program      program ,
                                    cl_uint         num_kernels ,
                                    cl_kernel *     kernels ,
                                    cl_uint *       num_kernels_ret)
{
    unsigned int i;
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint n;
    unsigned int comm = ocland_clCreateKernelsInProgram;
    if(num_kernels_ret) *num_kernels_ret=0;
    // Get the server
    int *sockfd = program->socket;
    if(!sockfd){
        return CL_INVALID_CONTEXT;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(program->ptr), sizeof(cl_program), MSG_MORE);
    Send(sockfd, &num_kernels, sizeof(cl_uint), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    Recv(sockfd, &n, sizeof(cl_uint), MSG_WAITALL);
    if(num_kernels_ret) *num_kernels_ret=n;
    if(kernels){
        if(num_kernels < n)
            n = num_kernels;
        Recv(sockfd, kernels, n*sizeof(cl_kernel), MSG_WAITALL);
        for(i=0;i<n;i++){
            addShortcut((void*)kernels[i], sockfd);
        }
    }
    return CL_SUCCESS;
}

cl_int oclandRetainKernel(cl_kernel     kernel)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clRetainKernel;
    // Get the server
    int *sockfd = kernel->socket;
    if(!sockfd){
        return CL_INVALID_KERNEL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(kernel->ptr), sizeof(cl_kernel), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_int oclandReleaseKernel(cl_kernel    kernel)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clReleaseKernel;
    // Get the server
    int *sockfd = kernel->socket;
    if(!sockfd){
        return CL_INVALID_KERNEL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(kernel->ptr), sizeof(cl_kernel), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag == CL_SUCCESS)
        delShortcut(kernel->ptr);
    return flag;
}

cl_int oclandSetKernelArg(cl_kernel     kernel ,
                          cl_uint       arg_index ,
                          size_t        arg_size ,
                          const void *  arg_value)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clSetKernelArg;
    // Get the server
    int *sockfd = kernel->socket;
    if(!sockfd){
        return CL_INVALID_KERNEL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(kernel->ptr), sizeof(cl_kernel), MSG_MORE);
    Send(sockfd, &arg_index, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, &arg_size, sizeof(size_t), MSG_MORE);
    if(arg_value){
        Send(sockfd, &arg_size, sizeof(size_t), MSG_MORE);
        Send(sockfd, arg_value, arg_size, 0);
    }
    else{  // local memory
        size_t null_size=0;
        Send(sockfd, &null_size, sizeof(size_t), 0);
    }
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_int oclandGetKernelInfo(cl_kernel        kernel ,
                           cl_kernel_info   param_name ,
                           size_t           param_value_size ,
                           void *           param_value ,
                           size_t *         param_value_size_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    size_t size_ret=0;
    unsigned int comm = ocland_clGetKernelInfo;
    if(param_value_size_ret) *param_value_size_ret=0;
    // Get the server
    int *sockfd = kernel->socket;
    if(!sockfd){
        return CL_INVALID_KERNEL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(kernel->ptr), sizeof(cl_kernel), MSG_MORE);
    Send(sockfd, &param_name, sizeof(cl_kernel_info), MSG_MORE);
    Send(sockfd, &param_value_size, sizeof(size_t), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    Recv(sockfd, &size_ret, sizeof(size_t), MSG_WAITALL);
    if(param_value_size_ret) *param_value_size_ret = size_ret;
    if(param_value){
        Recv(sockfd, param_value, size_ret, MSG_WAITALL);
    }
    return CL_SUCCESS;
}

cl_int oclandGetKernelWorkGroupInfo(cl_kernel                   kernel ,
                                    cl_device_id                device ,
                                    cl_kernel_work_group_info   param_name ,
                                    size_t                      param_value_size ,
                                    void *                      param_value ,
                                    size_t *                    param_value_size_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    size_t size_ret=0;
    unsigned int comm = ocland_clGetKernelWorkGroupInfo;
    if(param_value_size_ret) *param_value_size_ret=0;
    // Get the server
    int *sockfd = kernel->socket;
    if(!sockfd){
        return CL_INVALID_KERNEL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(kernel->ptr), sizeof(cl_kernel), MSG_MORE);
    Send(sockfd, &(device->ptr), sizeof(cl_device_id), MSG_MORE);
    Send(sockfd, &param_name, sizeof(cl_kernel_work_group_info), MSG_MORE);
    Send(sockfd, &param_value_size, sizeof(size_t), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    Recv(sockfd, &size_ret, sizeof(size_t), MSG_WAITALL);
    if(param_value_size_ret) *param_value_size_ret = size_ret;
    if(param_value){
        Recv(sockfd, param_value, size_ret, MSG_WAITALL);
    }
    return CL_SUCCESS;
}

cl_int oclandWaitForEvents(cl_uint              num_events ,
                           const cl_event *     event_list)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clWaitForEvents;
    // Get the server
    int *sockfd = event_list[0]->socket;
    if(!sockfd){
        return CL_INVALID_EVENT;
    }
    // Substitute the local references to the remote ones
    cl_event *events = calloc(num_events, sizeof(cl_event));
    if ((num_events > 0) && (NULL == events)){
        return CL_OUT_OF_HOST_MEMORY;
    }
    for(i=0;i<num_events;i++){
        events[i] = event_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &num_events, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, events, num_events*sizeof(cl_event), 0);
    free(events); events = NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_int oclandGetEventInfo(cl_event          event ,
                          cl_event_info     param_name ,
                          size_t            param_value_size ,
                          void *            param_value ,
                          size_t *          param_value_size_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    size_t size_ret=0;
    unsigned int comm = ocland_clGetEventInfo;
    if(param_value_size_ret) *param_value_size_ret=0;
    // Get the server
    int *sockfd = event->socket;
    if(!sockfd){
        return CL_INVALID_EVENT;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(event->ptr), sizeof(cl_event), MSG_MORE);
    Send(sockfd, &param_name, sizeof(cl_event_info), MSG_MORE);
    Send(sockfd, &param_value_size, sizeof(size_t), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    Recv(sockfd, &size_ret, sizeof(size_t), MSG_WAITALL);
    if(param_value_size_ret) *param_value_size_ret = size_ret;
    if(param_value){
        Recv(sockfd, param_value, size_ret, MSG_WAITALL);
    }
    return CL_SUCCESS;
}

cl_int oclandReleaseEvent(cl_event  event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clReleaseEvent;
    // Get the server
    int *sockfd = event->socket;
    if(!sockfd){
        return CL_INVALID_EVENT;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(event->ptr), sizeof(cl_event), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag == CL_SUCCESS)
        delShortcut(event->ptr);
    return flag;
}

cl_int oclandGetEventProfilingInfo(cl_event             event ,
                                   cl_profiling_info    param_name ,
                                   size_t               param_value_size ,
                                   void *               param_value ,
                                   size_t *             param_value_size_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    size_t size_ret=0;
    unsigned int comm = ocland_clGetEventProfilingInfo;
    if(param_value_size_ret) *param_value_size_ret=0;
    // Get the server
    int *sockfd = event->socket;
    if(!sockfd){
        return CL_INVALID_EVENT;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(event->ptr), sizeof(cl_event), MSG_MORE);
    Send(sockfd, &param_name, sizeof(cl_profiling_info), MSG_MORE);
    Send(sockfd, &param_value_size, sizeof(size_t), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    Recv(sockfd, &size_ret, sizeof(size_t), MSG_WAITALL);
    if(param_value_size_ret) *param_value_size_ret = size_ret;
    if(param_value){
        Recv(sockfd, param_value, size_ret, MSG_WAITALL);
    }
    return CL_SUCCESS;
}

cl_int oclandFlush(cl_command_queue command_queue)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clFlush;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_int oclandFinish(cl_command_queue  command_queue)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clFinish;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

/** @struct dataTransfer Vars needed for
 * an asynchronously data transfer.
 */
struct dataTransfer{
    /// Port
    unsigned int port;
    /// Socket
    int fd;
    /// Size of data
    size_t cb;
    /// Data array
    void *ptr;
};

/** Thread that receives data from server.
 * @param data struct dataTransfer casted variable.
 * @return NULL
 */
void *asyncDataRecv_thread(void *data)
{
    struct dataTransfer* _data = (struct dataTransfer*)data;
    // Connect to the received port.
    unsigned int port = _data->port;
    struct sockaddr_in serv_addr;
    //! @todo set SO_PRIORITY
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        printf("ERROR: Can't register a new socket for the asynchronous data transfer\n"); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    const char* ip = oclandServerAddress(_data->fd);
    if(!ip){
        printf("ERROR: Can't find the server associated with the socket\n"); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0){
        // we can't work, disconnect from server
        printf("ERROR: Invalid address assigment (%s)\n", ip); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    while( connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ){
        if(errno == ECONNREFUSED){
            // Pobably the server is not ready yet, simply retry
            // it until the server start listening
            continue;
        }
        // we can't work, disconnect from server
        printf("ERROR: Can't connect for the asynchronous data transfer\n"); fflush(stdout);
        printf("\t%s\n", strerror(errno)); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    dataPack in, out;
    out.size = _data->cb;
    out.data = _data->ptr;
    Recv(&fd, &(in.size), sizeof(size_t), MSG_WAITALL);
    if(in.size == 0){
        printf("Error uncompressing data:\n\tnull array size received"); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    in.data = malloc(in.size);
    Recv(&fd, in.data, in.size, MSG_WAITALL);
    unpack(out,in);
    free(in.data); in.data=NULL;
    THREAD_SAFE_EXIT;
}

/** Performs a data reception asynchronously on a new thread and socket.
 * @param sockfd Connection socket.
 * @param data Data to transfer.
 */
void asyncDataRecv(int* sockfd, struct dataTransfer data)
{
    // Open a new thread to connect to the new port
    // and receive the data
    pthread_t thread;
    struct dataTransfer* _data = (struct dataTransfer*)malloc(sizeof(struct dataTransfer));
    _data->port  = data.port;
    _data->fd    = data.fd;
    _data->cb    = data.cb;
    _data->ptr   = data.ptr;
    pthread_create(&thread, NULL, asyncDataRecv_thread, (void *)(_data));
}

cl_int oclandEnqueueReadBuffer(cl_command_queue     command_queue ,
                               cl_mem               buffer ,
                               cl_bool              blocking_read ,
                               size_t               offset ,
                               size_t               cb ,
                               void *               ptr ,
                               cl_uint              num_events_in_wait_list ,
                               const cl_event *     event_wait_list ,
                               cl_event *           event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueReadBuffer;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(buffer->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &blocking_read, sizeof(cl_bool), MSG_MORE);
    Send(sockfd, &offset, sizeof(size_t), MSG_MORE);
    Send(sockfd, &cb, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    // ------------------------------------------------------------
    // Blocking read case:
    // We may have received the flag, the event, and the data.
    // ------------------------------------------------------------
    if(blocking_read){
        if(event){
            Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
            addShortcut(*event, sockfd);
        }
        dataPack in, out;
        out.size = cb;
        out.data = ptr;
        Recv(sockfd, &(in.size), sizeof(size_t), MSG_WAITALL);
        in.data = malloc(in.size);
        Recv(sockfd, in.data, in.size, MSG_WAITALL);
        unpack(out,in);
        free(in.data); in.data=NULL;
        return CL_SUCCESS;
    }
    // ------------------------------------------------------------
    // Asynchronous read case:
    // We may have received the flag, the event, and a port to open
    // a parallel transfer channel.
    // ------------------------------------------------------------
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    unsigned int port;
    Recv(sockfd, &port, sizeof(unsigned int), MSG_WAITALL);
    struct dataTransfer data;
    data.port  = port;
    data.fd    = *sockfd;
    data.cb    = cb;
    data.ptr   = ptr;
    asyncDataRecv(sockfd, data);
    return CL_SUCCESS;
}

/** Thread that sends data to server.
 * @param data struct dataTransfer casted variable.
 * @return NULL
 */
void *asyncDataSend_thread(void *data)
{
    struct dataTransfer* _data = (struct dataTransfer*)data;
    // Connect to the received port.
    unsigned int port = _data->port;
    struct sockaddr_in serv_addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        printf("ERROR: Can't register a new socket for the asynchronous data transfer\n"); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    const char* ip = oclandServerAddress(_data->fd);
    if(!ip){
        printf("ERROR: Can't find the server associated with the socket\n"); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0){
        // we can't work, disconnect from server
        printf("ERROR: Invalid address assigment (%s)\n", ip); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    while( connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ){
        if(errno == ECONNREFUSED){
            // Pobably the server is not ready yet, simply retry
            // it until the server start listening
            continue;
        }
        // we can't work, disconnect from server
        printf("ERROR: Can't connect for the asynchronous data transfer\n"); fflush(stdout);
        printf("\t%s\n", strerror(errno)); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    // Return the data (compressed) to the client
    dataPack in, out;
    in.size = _data->cb;
    in.data = _data->ptr;
    out = pack(in);
    // Since the array size is not the original one anymore, we need to
    // send the array size before to send the data
    Send(&fd, &(out.size), sizeof(size_t), MSG_MORE);
    Send(&fd, out.data, out.size, 0);
    // Clean up
    free(out.data); out.data = NULL;
    THREAD_SAFE_EXIT;
}

/** Performs a data reception asynchronously on a new thread and socket.
 * @param sockfd Connection socket.
 * @param data Data to transfer.
 */
void asyncDataSend(int* sockfd, struct dataTransfer data)
{
    // Open a new thread to connect to the new port
    // and receive the data
    pthread_t thread;
    struct dataTransfer* _data = (struct dataTransfer*)malloc(sizeof(struct dataTransfer));
    _data->port  = data.port;
    _data->fd    = data.fd;
    _data->cb    = data.cb;
    _data->ptr   = data.ptr;
    pthread_create(&thread, NULL, asyncDataSend_thread, (void *)(_data));
}

cl_int oclandEnqueueWriteBuffer(cl_command_queue    command_queue ,
                                cl_mem              buffer ,
                                cl_bool             blocking_write ,
                                size_t              offset ,
                                size_t              cb ,
                                const void *        ptr ,
                                cl_uint             num_events_in_wait_list ,
                                const cl_event *    event_wait_list ,
                                cl_event *          event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueWriteBuffer;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(buffer->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &blocking_write, sizeof(cl_bool), MSG_MORE);
    Send(sockfd, &offset, sizeof(size_t), MSG_MORE);
    Send(sockfd, &cb, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    int ending = 0;
    if(blocking_write) ending = MSG_MORE;
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), ending);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), ending);
    }
    free(events_wait); events_wait=NULL;
    if(blocking_write){
        dataPack in, out;
        in.size = cb;
        in.data = (void *)ptr;
        out = pack(in);
        Send(sockfd, &(out.size), sizeof(size_t), MSG_MORE);
        Send(sockfd, out.data, out.size, 0);
        free(out.data); out.data = NULL;
    }
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    // ------------------------------------------------------------
    // Blocking read case:
    // We may have received the flag and the event.
    // ------------------------------------------------------------
    if(blocking_write){
        if(event){
            Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
            addShortcut(*event, sockfd);
        }
        return CL_SUCCESS;
    }
    // ------------------------------------------------------------
    // Asynchronous read case:
    // We may have received the flag, the event, and a port to open
    // a parallel transfer channel.
    // ------------------------------------------------------------
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    unsigned int port;
    Recv(sockfd, &port, sizeof(unsigned int), MSG_WAITALL);
    struct dataTransfer data;
    data.port  = port;
    data.fd    = *sockfd;
    data.cb    = cb;
    data.ptr   = (void*)ptr;
    asyncDataSend(sockfd, data);
    return flag;
}

cl_int oclandEnqueueCopyBuffer(cl_command_queue     command_queue ,
                               cl_mem               src_buffer ,
                               cl_mem               dst_buffer ,
                               size_t               src_offset ,
                               size_t               dst_offset ,
                               size_t               cb ,
                               cl_uint              num_events_in_wait_list ,
                               const cl_event *     event_wait_list ,
                               cl_event *           event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueCopyBuffer;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(src_buffer->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &(dst_buffer->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &src_offset, sizeof(size_t), MSG_MORE);
    Send(sockfd, &dst_offset, sizeof(size_t), MSG_MORE);
    Send(sockfd, &cb, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

cl_int oclandEnqueueCopyImage(cl_command_queue      command_queue ,
                              cl_mem                src_image ,
                              cl_mem                dst_image ,
                              const size_t *        src_origin ,
                              const size_t *        dst_origin ,
                              const size_t *        region ,
                              cl_uint               num_events_in_wait_list ,
                              const cl_event *      event_wait_list ,
                              cl_event *            event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueCopyImage;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(src_image->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &(dst_image->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, src_origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, dst_origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, region, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

cl_int oclandEnqueueCopyImageToBuffer(cl_command_queue  command_queue ,
                                      cl_mem            src_image ,
                                      cl_mem            dst_buffer ,
                                      const size_t *    src_origin ,
                                      const size_t *    region ,
                                      size_t            dst_offset ,
                                      cl_uint           num_events_in_wait_list ,
                                      const cl_event *  event_wait_list ,
                                      cl_event *        event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueCopyImageToBuffer;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(src_image->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &(dst_buffer->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, src_origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, region, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, &dst_offset, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

cl_int oclandEnqueueCopyBufferToImage(cl_command_queue  command_queue ,
                                      cl_mem            src_buffer ,
                                      cl_mem            dst_image ,
                                      size_t            src_offset ,
                                      const size_t *    dst_origin ,
                                      const size_t *    region ,
                                      cl_uint           num_events_in_wait_list ,
                                      const cl_event *  event_wait_list ,
                                      cl_event *        event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueCopyBufferToImage;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(src_buffer->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &(dst_image->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &src_offset, sizeof(size_t), MSG_MORE);
    Send(sockfd, dst_origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, region, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

cl_int oclandEnqueueNDRangeKernel(cl_command_queue  command_queue ,
                                  cl_kernel         kernel ,
                                  cl_uint           work_dim ,
                                  const size_t *    global_work_offset ,
                                  const size_t *    global_work_size ,
                                  const size_t *    local_work_size ,
                                  cl_uint           num_events_in_wait_list ,
                                  const cl_event *  event_wait_list ,
                                  cl_event *        event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueNDRangeKernel;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    cl_bool has_global_work_offset = CL_FALSE;
    if(global_work_offset) has_global_work_offset = CL_TRUE;
    cl_bool has_local_work_size = CL_FALSE;
    if(local_work_size) has_local_work_size = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(kernel->ptr), sizeof(cl_kernel), MSG_MORE);
    Send(sockfd, &work_dim, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, &has_global_work_offset, sizeof(cl_bool), MSG_MORE);
    Send(sockfd, &has_local_work_size, sizeof(cl_bool), MSG_MORE);
    if(has_global_work_offset)
        Send(sockfd, global_work_offset, work_dim*sizeof(size_t), MSG_MORE);
    Send(sockfd, global_work_size, work_dim*sizeof(size_t), MSG_MORE);
    if(has_local_work_size)
        Send(sockfd, local_work_size, work_dim*sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

/** @struct dataTransferRect Vars needed for
 * an asynchronously data transfer in 2D,3D
 * rectangle.
 */
struct dataTransferRect{
    /// Port
    unsigned int port;
    /// Socket
    int fd;
    /// Region to read
    const size_t *region;
    /// Size of a row
    size_t row;
    /// Size of a 2D slice (row*column)
    size_t slice;
    /// Size of data
    size_t cb;
    /// Data array (conviniently sifted with origin)
    void *ptr;
};

/** Thread that receives data from server for
 * a clEnqueueReadBufferRect specific command.
 * @param data struct dataTransfer casted variable.
 * @return NULL
 */
void *asyncDataRecvRect_thread(void *data)
{
    struct dataTransferRect* _data = (struct dataTransferRect*)data;
    // Connect to the received port.
    unsigned int port = _data->port;
    struct sockaddr_in serv_addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        printf("ERROR: Can't register a new socket for the asynchronous data transfer\n"); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    const char* ip = oclandServerAddress(_data->fd);
    if(!ip){
        printf("ERROR: Can't find the server associated with the socket\n"); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0){
        // we can't work, disconnect from server
        printf("ERROR: Invalid address assigment (%s)\n", ip); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    while( connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ){
        if(errno == ECONNREFUSED){
            // Pobably the server is not ready yet, simply retry
            // it until the server start listening
            continue;
        }
        // we can't work, disconnect from server
        printf("ERROR: Can't connect for the asynchronous data transfer\n"); fflush(stdout);
        printf("\t%s\n", strerror(errno)); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    dataPack in, out;
    out.size = _data->cb;
    out.data = malloc(out.size);
    Recv(&fd, &(in.size), sizeof(size_t), MSG_WAITALL);
    if(in.size == 0){
        printf("Error uncompressing data:\n\tnull array size received"); fflush(stdout);
        free(_data); _data=NULL;
        THREAD_SAFE_EXIT;
    }
    in.data = malloc(in.size);
    Recv(&fd, in.data, in.size, MSG_WAITALL);
    unpack(out,in);
    free(in.data); in.data=NULL;

    size_t i1, i2;
    for(i2 = 0; i2 < _data->region[2]; i2++) {
        for(i1 = 0; i1 < _data->region[1]; i1++) {
            char *dest = (char*)_data->ptr + i1 * _data->row + i2 * _data->slice;
            char *src = (char*)out.data + i1 * _data->region[0] + i2 * _data->region[0] * _data->region[1];
            memcpy(dest, src, _data->region[0]);
        }
    }
    free(out.data); out.data = NULL;
    THREAD_SAFE_EXIT;
}

/** Performs a data reception asynchronously on a new thread and socket, for
 * a clEnqueueReadBufferRect specific command.
 * @param sockfd Connection socket.
 * @param data Data to transfer.
 */
void asyncDataRecvRect(int* sockfd, struct dataTransferRect data)
{
    // Open a new thread to connect to the new port
    // and receive the data
    pthread_t thread;
    struct dataTransferRect* _data = (struct dataTransferRect*)malloc(sizeof(struct dataTransferRect));
    _data->port  = data.port;
    _data->fd    = data.fd;
    _data->region = data.region;
    _data->row    = data.row;
    _data->slice  = data.slice;
    _data->cb    = data.cb;
    _data->ptr   = data.ptr;
    pthread_create(&thread, NULL, asyncDataRecvRect_thread, (void *)(_data));
}

cl_int oclandEnqueueReadImage(cl_command_queue      command_queue ,
                              cl_mem                image ,
                              cl_bool               blocking_read ,
                              const size_t *        origin ,
                              const size_t *        region ,
                              size_t                row_pitch ,
                              size_t                slice_pitch ,
                              size_t                element_size ,
                              void *                ptr ,
                              cl_uint               num_events_in_wait_list ,
                              const cl_event *      event_wait_list ,
                              cl_event *            event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueReadImage;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    size_t cb = region[2]*slice_pitch + region[1]*row_pitch + region[0]*element_size;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(image->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &blocking_read, sizeof(cl_bool), MSG_MORE);
    Send(sockfd, origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, region, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, &row_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &slice_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &element_size, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    // ------------------------------------------------------------
    // Blocking read case:
    // We may have received the flag, the event, and the data.
    // ------------------------------------------------------------
    if(blocking_read){
        if(event){
            Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
            addShortcut(*event, sockfd);
        }
        dataPack in, out;
        out.size = cb;
        out.data = ptr;
        Recv(sockfd, &(in.size), sizeof(size_t), MSG_WAITALL);
        in.data = malloc(in.size);
        Recv(sockfd, in.data, in.size, MSG_WAITALL);
        unpack(out,in);
        free(in.data); in.data=NULL;
        return CL_SUCCESS;
    }
    // ------------------------------------------------------------
    // Asynchronous read case:
    // We may have received the flag, the event, and a port to open
    // a parallel transfer channel.
    // ------------------------------------------------------------
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    unsigned int port;
    Recv(sockfd, &port, sizeof(unsigned int), MSG_WAITALL);
    struct dataTransferRect data;
    data.port   = port;
    data.fd     = *sockfd;
    data.region = region;
    data.row    = row_pitch;
    data.slice  = slice_pitch;
    data.cb     = cb;
    data.ptr    = ptr;
    asyncDataRecvRect(sockfd, data);
    return CL_SUCCESS;
}

/** Thread that sends data to server.
 * @param data struct dataTransfer casted variable.
 * @return NULL
 */
void *asyncDataSendRect_thread(void *data)
{
    struct dataTransferRect* _data = (struct dataTransferRect*)data;
    // Connect to the received port.
    unsigned int port = _data->port;
    struct sockaddr_in serv_addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        printf("ERROR: Can't register a new socket for the asynchronous data transfer\n"); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    const char* ip = oclandServerAddress(_data->fd);
    if(!ip){
        printf("ERROR: Can't find the server associated with the socket\n"); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0){
        // we can't work, disconnect from server
        printf("ERROR: Invalid address assigment (%s)\n", ip); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    while( connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ){
        if(errno == ECONNREFUSED){
            // Pobably the server is not ready yet, simply retry
            // it until the server start listening
            continue;
        }
        // we can't work, disconnect from server
        printf("ERROR: Can't connect for the asynchronous data transfer\n"); fflush(stdout);
        printf("\t%s\n", strerror(errno)); fflush(stdout);
        THREAD_SAFE_EXIT;
    }
    // Return the data (compressed) to the client
    dataPack in, out;
    in.size = _data->cb;
    in.data = _data->ptr;
    out = pack(in);
    // Since the array size is not the original one anymore, we need to
    // send the array size before to send the data
    Send(&fd, &(out.size), sizeof(size_t), MSG_MORE);
    Send(&fd, out.data, out.size, 0);
    // Clean up
    free(out.data); out.data = NULL;
    THREAD_SAFE_EXIT;
}

/** Performs a data reception asynchronously on a new thread and socket.
 * @param sockfd Connection socket.
 * @param data Data to transfer.
 */
void asyncDataSendRect(int* sockfd, struct dataTransferRect data)
{
    // Open a new thread to connect to the new port
    // and receive the data
    pthread_t thread;
    struct dataTransferRect* _data = (struct dataTransferRect*)malloc(sizeof(struct dataTransferRect));
    _data->port  = data.port;
    _data->fd    = data.fd;
    _data->region = data.region;
    _data->row    = data.row;
    _data->slice  = data.slice;
    _data->cb    = data.cb;
    _data->ptr   = data.ptr;
    pthread_create(&thread, NULL, asyncDataSendRect_thread, (void *)(_data));
}

cl_int oclandEnqueueWriteImage(cl_command_queue     command_queue ,
                               cl_mem               image ,
                               cl_bool              blocking_write ,
                               const size_t *       origin ,
                               const size_t *       region ,
                               size_t               row_pitch ,
                               size_t               slice_pitch ,
                               size_t               element_size ,
                               const void *         ptr ,
                               cl_uint              num_events_in_wait_list ,
                               const cl_event *     event_wait_list ,
                               cl_event *           event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueWriteImage;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    size_t cb = region[2]*slice_pitch + region[1]*row_pitch + region[0]*element_size;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &command_queue, sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &image, sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &blocking_write, sizeof(cl_bool), MSG_MORE);
    Send(sockfd, origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, region, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, &row_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &slice_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &element_size, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    int ending = 0;
    if(blocking_write) ending = MSG_MORE;
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), ending);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), ending);
    }
    free(events_wait); events_wait=NULL;
    if(blocking_write){
        dataPack in, out;
        in.size = cb;
        in.data = (void*)ptr;
        out = pack(in);
        Send(sockfd, &(out.size), sizeof(size_t), MSG_MORE);
        Send(sockfd, out.data, out.size, 0);
        free(out.data); out.data = NULL;
    }
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    // ------------------------------------------------------------
    // Blocking read case:
    // We may have received the flag and the event.
    // ------------------------------------------------------------
    if(blocking_write){
        if(event){
            Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
            addShortcut(*event, sockfd);
        }
        return CL_SUCCESS;
    }
    // ------------------------------------------------------------
    // Asynchronous read case:
    // We may have received the flag, the event, and a port to open
    // a parallel transfer channel.
    // ------------------------------------------------------------
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    unsigned int port;
    Recv(sockfd, &port, sizeof(unsigned int), MSG_WAITALL);
    struct dataTransferRect data;
    data.port  = port;
    data.fd    = *sockfd;
    data.region = region;
    data.row    = row_pitch;
    data.slice  = slice_pitch;
    data.cb    = cb;
    data.ptr   = (void*)ptr;
    asyncDataSendRect(sockfd, data);
    return flag;
}

// -------------------------------------------- //
//                                              //
// OpenCL 1.1 methods                           //
//                                              //
// -------------------------------------------- //

cl_event oclandCreateUserEvent(cl_context     context ,
                               cl_int *       errcode_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_event event = NULL;
    unsigned int comm = ocland_clCreateUserEvent;
    if(errcode_ret) *errcode_ret = CL_SUCCESS;
    // Get the server
    int *sockfd = context->server->socket;
    if(!sockfd){
        if(errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(context->ptr), sizeof(cl_context), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        if(errcode_ret) *errcode_ret = flag;
        return NULL;
    }
    Recv(sockfd, &event, sizeof(cl_event), MSG_WAITALL);
    addShortcut((void*)event, sockfd);
    return event;
}

cl_int oclandSetUserEventStatus(cl_event    event ,
                                cl_int      execution_status)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clSetUserEventStatus;
    // Get the server
    int *sockfd = event->socket;
    if(!sockfd){
        return CL_INVALID_EVENT;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(event->ptr), sizeof(cl_event), 0);
    Send(sockfd, &execution_status, sizeof(cl_int), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_int oclandEnqueueReadBufferRect(cl_command_queue     command_queue ,
                                   cl_mem               mem ,
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
                                   cl_event *           event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueReadBufferRect;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    size_t origin = host_origin[0] + host_origin[1]*host_row_pitch + host_origin[2]*host_slice_pitch;
    ptr = (char*)ptr + origin;
    size_t cb = region[0] * region[1] * region[2];
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(mem->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &blocking_read, sizeof(cl_bool), MSG_MORE);
    Send(sockfd, buffer_origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, region, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, &buffer_row_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &buffer_slice_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    // ------------------------------------------------------------
    // Blocking read case:
    // We may have received the flag, the event, and the data.
    // ------------------------------------------------------------
    if(blocking_read){
        if(event){
            Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
            addShortcut(*event, sockfd);
        }
        dataPack in, out;
        out.size = cb;
        out.data = malloc(out.size);
        Recv(sockfd, &(in.size), sizeof(size_t), MSG_WAITALL);
        in.data = malloc(in.size);
        Recv(sockfd, in.data, in.size, MSG_WAITALL);
        unpack(out,in);
        free(in.data); in.data = NULL;

        size_t i1, i2;
        for(i2 = 0; i2 < region[2]; i2++) {
            for(i1 = 0; i1 < region[1]; i1++) {
                char* dest = (char*)ptr + i1 * host_row_pitch + i2 * host_slice_pitch;
                char* src = (char*)out.data + i1 * region[0] + i2 * region[0] * region[1];
                memcpy(dest, src, region[0]);
            }
        }
        free(out.data); out.data = NULL;
        return CL_SUCCESS;
    }
    // ------------------------------------------------------------
    // Asynchronous read case:
    // We may have received the flag, the event, and a port to open
    // a parallel transfer channel.
    // ------------------------------------------------------------
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    unsigned int port;
    Recv(sockfd, &port, sizeof(unsigned int), MSG_WAITALL);
    struct dataTransferRect data;
    data.port   = port;
    data.fd     = *sockfd;
    data.region = region;
    data.row    = host_row_pitch;
    data.slice  = host_slice_pitch;
    data.cb     = cb;
    data.ptr    = ptr;
    asyncDataRecvRect(sockfd, data);
    return CL_SUCCESS;
}

cl_int oclandEnqueueWriteBufferRect(cl_command_queue     command_queue ,
                                    cl_mem               mem ,
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
                                    cl_event *           event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueWriteImage;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    size_t cb = region[2]*host_slice_pitch + region[1]*host_row_pitch + region[0];
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(mem->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &blocking_write, sizeof(cl_bool), MSG_MORE);
    Send(sockfd, buffer_origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, region, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, &buffer_row_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &buffer_slice_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &host_row_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &host_slice_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    int ending = 0;
    if(blocking_write) ending = MSG_MORE;
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), ending);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), ending);
    }
    free(events_wait); events_wait=NULL;
    if(blocking_write){
        dataPack in, out;
        in.size = cb;
        in.data = (void *)ptr;
        out = pack(in);
        Send(sockfd, &(out.size), sizeof(size_t), MSG_MORE);
        Send(sockfd, out.data, out.size, 0);
        free(out.data); out.data = NULL;
    }
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    // ------------------------------------------------------------
    // Blocking read case:
    // We may have received the flag and the event.
    // ------------------------------------------------------------
    if(blocking_write){
        if(event){
            Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
            addShortcut(*event, sockfd);
        }
        return CL_SUCCESS;
    }
    // ------------------------------------------------------------
    // Asynchronous read case:
    // We may have received the flag, the event, and a port to open
    // a parallel transfer channel.
    // ------------------------------------------------------------
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    unsigned int port;
    Recv(sockfd, &port, sizeof(unsigned int), MSG_WAITALL);

    struct dataTransferRect data;
    data.port   = port;
    data.fd     = *sockfd;
    data.region = region;
    data.row    = host_row_pitch;
    data.slice  = host_slice_pitch;
    data.cb     = cb;
    data.ptr    = (void*)ptr;
    asyncDataSendRect(sockfd, data);
    return flag;
}

cl_int oclandEnqueueCopyBufferRect(cl_command_queue     command_queue ,
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
                                   cl_event *           event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueCopyBufferRect;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(src_buffer->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &(dst_buffer->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, src_origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, dst_origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, region, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, &src_row_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &src_slice_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &dst_row_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &dst_slice_pitch, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

// -------------------------------------------- //
//                                              //
// OpenCL 1.2 methods                           //
//                                              //
// -------------------------------------------- //

cl_program oclandCreateProgramWithBuiltInKernels(cl_context             context ,
                                                 cl_uint                num_devices ,
                                                 const cl_device_id *   device_list ,
                                                 const char *           kernel_names ,
                                                 cl_int *               errcode_ret)
{
    unsigned int i;
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_program program = NULL;
    unsigned int comm = ocland_clCreateProgramWithBuiltInKernels;
    size_t kernel_names_size = (strlen(kernel_names) + 1)*sizeof(char);
    if(errcode_ret) *errcode_ret = CL_SUCCESS;
    // Get the server
    int *sockfd = context->server->socket;
    if(!sockfd){
        if(errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    // Substitute the local references to the remote ones
    cl_device_id *devices = calloc(num_devices, sizeof(cl_device_id));
    if ((num_devices > 0) && (NULL == devices)){
        if (errcode_ret) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    for(i=0;i<num_devices;i++){
        devices[i] = device_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(context->ptr), sizeof(cl_context), MSG_MORE);
    Send(sockfd, &num_devices, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, devices, num_devices*sizeof(cl_device_id), MSG_MORE);
    Send(sockfd, &kernel_names_size, sizeof(size_t), MSG_MORE);
    Send(sockfd, kernel_names, kernel_names_size, 0);
    free(devices); devices = NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        if(errcode_ret) *errcode_ret = flag;
        return NULL;
    }
    Recv(sockfd, &program, sizeof(cl_program), MSG_WAITALL);
    addShortcut((void*)program, sockfd);
    return program;
}

cl_int oclandCompileProgram(cl_program            program ,
                            cl_uint               num_devices ,
                            const cl_device_id *  device_list ,
                            const char *          options ,
                            cl_uint               num_input_headers ,
                            const cl_program *    input_headers,
                            const char **         header_include_names ,
                            void (CL_CALLBACK *   pfn_notify)(cl_program  program , void *  user_data),
                            void *                user_data)
{
    unsigned int i;
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clCompileProgram;
    size_t str_size = (strlen(options) + 1)*sizeof(char);
    // Get the server
    int *sockfd = getShortcut(program);
    if(!sockfd){
        return CL_INVALID_PROGRAM;
    }
    // Substitute the local references to the remote ones
    cl_device_id *devices = calloc(num_devices, sizeof(cl_device_id));
    if ((num_devices > 0) && (NULL == devices)){
        return CL_OUT_OF_HOST_MEMORY;
    }
    for(i=0;i<num_devices;i++){
        devices[i] = device_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(program->ptr), sizeof(cl_program), MSG_MORE);
    Send(sockfd, &num_devices, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, devices, num_devices*sizeof(cl_device_id), MSG_MORE);
    Send(sockfd, &str_size, sizeof(size_t), MSG_MORE);
    Send(sockfd, options, str_size, MSG_MORE);
    if(num_input_headers){
        Send(sockfd, &num_input_headers, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, input_headers, num_input_headers*sizeof(cl_program), MSG_MORE);
        for(i=0;i<num_input_headers-1;i++){
            str_size = (strlen(header_include_names[i]) + 1)*sizeof(char);
            Send(sockfd, &str_size, sizeof(size_t), MSG_MORE);
            Send(sockfd, header_include_names[i], str_size, MSG_MORE);
        }
        str_size = (strlen(header_include_names[i]) + 1)*sizeof(char);
        Send(sockfd, &str_size, sizeof(size_t), MSG_MORE);
        Send(sockfd, header_include_names[i], str_size, 0);
    }
    else{
        Send(sockfd, &num_input_headers, sizeof(cl_uint), 0);
    }
    free(devices); devices = NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_program oclandLinkProgram(cl_context            context ,
                             cl_uint               num_devices ,
                             const cl_device_id *  device_list ,
                             const char *          options ,
                             cl_uint               num_input_programs ,
                             const cl_program *    input_programs ,
                             void (CL_CALLBACK *   pfn_notify)(cl_program  program , void *  user_data),
                             void *                user_data ,
                             cl_int *              errcode_ret)
{
    unsigned int i;
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_program program=NULL;
    unsigned int comm = ocland_clLinkProgram;
    if(errcode_ret) *errcode_ret = CL_SUCCESS;
    size_t str_size = (strlen(options) + 1)*sizeof(char);
    // Get the server
    int *sockfd = context->server->socket;
    if(!sockfd){
        if(errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    // Substitute the local references to the remote ones
    cl_device_id *devices = calloc(num_devices, sizeof(cl_device_id));
    if ((num_devices > 0) && (NULL == devices)) {
        if (errcode_ret) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    for(i=0;i<num_devices;i++){
        devices[i] = device_list[i]->ptr;
    }
    cl_program *programs = calloc(num_input_programs, sizeof(cl_program));
    if ((num_input_programs > 0) && (NULL == programs)){
        if (errcode_ret) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return NULL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(context->ptr), sizeof(cl_context), MSG_MORE);
    Send(sockfd, &num_devices, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, devices, num_devices*sizeof(cl_device_id), MSG_MORE);
    Send(sockfd, &str_size, sizeof(size_t), MSG_MORE);
    Send(sockfd, options, str_size, MSG_MORE);
    if(num_input_programs){
        for(i=0;i<num_input_programs;i++){
            programs[i] = input_programs[i]->ptr;
        }
        Send(sockfd, &num_input_programs, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, programs, num_input_programs*sizeof(cl_program), 0);
    }
    else{
        Send(sockfd, &num_input_programs, sizeof(cl_uint), 0);
    }
    free(devices); devices = NULL;
    free(programs); programs = NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        if(errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    return program;
}

cl_int oclandUnloadPlatformCompiler(cl_platform_id  platform)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    unsigned int comm = ocland_clUnloadPlatformCompiler;
    // Get the server
    int *sockfd = platform->server->socket;
    if(!sockfd){
        return CL_INVALID_PLATFORM;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(platform->ptr), sizeof(cl_platform_id), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    return flag;
}

cl_int oclandGetKernelArgInfo(cl_kernel            kernel ,
                              cl_uint              arg_index ,
                              cl_kernel_arg_info   param_name ,
                              size_t               param_value_size ,
                              void *               param_value ,
                              size_t *             param_value_size_ret)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    size_t size_ret=0;
    unsigned int comm = ocland_clGetKernelArgInfo;
    if(param_value_size_ret) *param_value_size_ret=0;
    // Get the server
    int *sockfd = kernel->socket;
    if(!sockfd){
        return CL_INVALID_KERNEL;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(kernel->ptr), sizeof(cl_kernel), MSG_MORE);
    Send(sockfd, &arg_index, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, &param_name, sizeof(cl_kernel_arg_info), MSG_MORE);
    Send(sockfd, &param_value_size, sizeof(size_t), 0);
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS){
        return flag;
    }
    Recv(sockfd, &size_ret, sizeof(size_t), MSG_WAITALL);
    if(param_value_size_ret) *param_value_size_ret = size_ret;
    if(param_value){
        Recv(sockfd, param_value, size_ret, MSG_WAITALL);
    }
    return CL_SUCCESS;
}

cl_int oclandEnqueueFillBuffer(cl_command_queue    command_queue ,
                               cl_mem              mem ,
                               const void *        pattern ,
                               size_t              pattern_size ,
                               size_t              offset ,
                               size_t              cb ,
                               cl_uint             num_events_in_wait_list ,
                               const cl_event *    event_wait_list ,
                               cl_event *          event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueFillBuffer;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &(mem->ptr), sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &pattern_size, sizeof(size_t), MSG_MORE);
    Send(sockfd, pattern, pattern_size, MSG_MORE);
    Send(sockfd, &offset, sizeof(size_t), MSG_MORE);
    Send(sockfd, &cb, sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

cl_int oclandEnqueueFillImage(cl_command_queue    command_queue ,
                              cl_mem              image ,
                              size_t              fill_color_size ,
                              const void *        fill_color ,
                              const size_t *      origin ,
                              const size_t *      region ,
                              cl_uint             num_events_in_wait_list ,
                              const cl_event *    event_wait_list ,
                              cl_event *          event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueFillImage;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &command_queue, sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &image, sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &fill_color_size, sizeof(size_t), MSG_MORE);
    Send(sockfd, fill_color, fill_color_size, MSG_MORE);
    Send(sockfd, origin, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, region, 3*sizeof(size_t), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

cl_int oclandEnqueueMigrateMemObjects(cl_command_queue        command_queue ,
                                      cl_uint                 num_mem_objects ,
                                      const cl_mem *          mem_objects ,
                                      cl_mem_migration_flags  flags ,
                                      cl_uint                 num_events_in_wait_list ,
                                      const cl_event *        event_wait_list ,
                                      cl_event *              event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueMigrateMemObjects;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events (and mems) from the local references to the remote ones
    cl_mem *mems = (cl_mem*)malloc(num_mem_objects*sizeof(cl_mem));
    if(!mems){
        return CL_OUT_OF_HOST_MEMORY;
    }
    for(i=0;i<num_mem_objects;i++)
        mems[i] = mem_objects[i]->ptr;
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &num_mem_objects, sizeof(cl_uint), MSG_MORE);
    Send(sockfd, mems, num_mem_objects*sizeof(cl_mem), MSG_MORE);
    Send(sockfd, &flags, sizeof(cl_mem_migration_flags), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(mems); mems=NULL;
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

cl_int oclandEnqueueMarkerWithWaitList(cl_command_queue  command_queue ,
                                       cl_uint            num_events_in_wait_list ,
                                       const cl_event *   event_wait_list ,
                                       cl_event *         event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueMarkerWithWaitList;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}

cl_int oclandEnqueueBarrierWithWaitList(cl_command_queue   command_queue ,
                                        cl_uint            num_events_in_wait_list ,
                                        const cl_event *   event_wait_list ,
                                        cl_event *         event)
{
    cl_int flag = CL_OUT_OF_RESOURCES;
    cl_uint i;
    unsigned int comm = ocland_clEnqueueBarrierWithWaitList;
    cl_bool want_event = CL_FALSE;
    if(event) want_event = CL_TRUE;
    // Get the server
    int *sockfd = command_queue->server->socket;
    if(!sockfd){
        return CL_INVALID_COMMAND_QUEUE;
    }
    // Change the events from the local references to the remote ones
    cl_event *events_wait = NULL;
    if(num_events_in_wait_list){
        events_wait = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        if(!events_wait){
            return CL_OUT_OF_HOST_MEMORY;
        }
        for(i=0;i<num_events_in_wait_list;i++)
            events_wait[i] = event_wait_list[i]->ptr;
    }
    // Send the command data
    Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    Send(sockfd, &(command_queue->ptr), sizeof(cl_command_queue), MSG_MORE);
    Send(sockfd, &want_event, sizeof(cl_bool), MSG_MORE);
    if(num_events_in_wait_list){
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_MORE);
        Send(sockfd, events_wait, num_events_in_wait_list*sizeof(cl_event), 0);
    }
    else{
        Send(sockfd, &num_events_in_wait_list, sizeof(cl_uint), 0);
    }
    free(events_wait); events_wait=NULL;
    // Receive the answer
    Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(flag != CL_SUCCESS)
        return flag;
    if(event){
        Recv(sockfd, event, sizeof(cl_event), MSG_WAITALL);
        addShortcut(*event, sockfd);
    }
    return CL_SUCCESS;
}
