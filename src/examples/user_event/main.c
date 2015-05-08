/*
 *  This file is part of ocland, a free cloud OpenCL interface.
 *  Copyright (C) 2015  Jose Luis Cercos Pita <jl.cercos@upm.es>
 *
 *  ocland is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ocland is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser Public License
 *  along with ocland.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <CL/opencl.h>

static char err_str[64];
const char* OpenCLError(cl_int err_code)
{
    switch(err_code){
    case CL_SUCCESS:
        strcpy(err_str, "CL_SUCCESS");
        break;
    case CL_DEVICE_NOT_FOUND:
        strcpy(err_str, "CL_DEVICE_NOT_FOUND");
        break;
    case CL_DEVICE_NOT_AVAILABLE:
        strcpy(err_str, "CL_DEVICE_NOT_AVAILABLE");
        break;
    case CL_COMPILER_NOT_AVAILABLE:
        strcpy(err_str, "CL_COMPILER_NOT_AVAILABLE");
        break;
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
        strcpy(err_str, "CL_MEM_OBJECT_ALLOCATION_FAILURE");
        break;
    case CL_OUT_OF_RESOURCES:
        strcpy(err_str, "CL_OUT_OF_RESOURCES");
        break;
    case CL_OUT_OF_HOST_MEMORY:
        strcpy(err_str, "CL_OUT_OF_HOST_MEMORY");
        break;
    case CL_PROFILING_INFO_NOT_AVAILABLE:
        strcpy(err_str, "CL_PROFILING_INFO_NOT_AVAILABLE");
        break;
    case CL_MEM_COPY_OVERLAP:
        strcpy(err_str, "CL_MEM_COPY_OVERLAP");
        break;
    case CL_IMAGE_FORMAT_MISMATCH:
        strcpy(err_str, "CL_IMAGE_FORMAT_MISMATCH");
        break;
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:
        strcpy(err_str, "CL_IMAGE_FORMAT_NOT_SUPPORTED");
        break;
    case CL_BUILD_PROGRAM_FAILURE:
        strcpy(err_str, "CL_BUILD_PROGRAM_FAILURE");
        break;
    case CL_MAP_FAILURE:
        strcpy(err_str, "CL_MAP_FAILURE");
        break;
    case CL_MISALIGNED_SUB_BUFFER_OFFSET:
        strcpy(err_str, "CL_MISALIGNED_SUB_BUFFER_OFFSET");
        break;
    case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
        strcpy(err_str, "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST");
        break;
    case CL_COMPILE_PROGRAM_FAILURE:
        strcpy(err_str, "CL_COMPILE_PROGRAM_FAILURE");
        break;
    case CL_LINKER_NOT_AVAILABLE:
        strcpy(err_str, "CL_LINKER_NOT_AVAILABLE");
        break;
    case CL_LINK_PROGRAM_FAILURE:
        strcpy(err_str, "CL_LINK_PROGRAM_FAILURE");
        break;
    case CL_DEVICE_PARTITION_FAILED:
        strcpy(err_str, "CL_DEVICE_PARTITION_FAILED");
        break;
    case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:
        strcpy(err_str, "CL_KERNEL_ARG_INFO_NOT_AVAILABLE");
        break;
    case CL_INVALID_VALUE:
        strcpy(err_str, "CL_INVALID_VALUE");
        break;
    case CL_INVALID_DEVICE_TYPE:
        strcpy(err_str, "CL_INVALID_DEVICE_TYPE");
        break;
    case CL_INVALID_PLATFORM:
        strcpy(err_str, "CL_INVALID_PLATFORM");
        break;
    case CL_INVALID_DEVICE:
        strcpy(err_str, "CL_INVALID_DEVICE");
        break;
    case CL_INVALID_CONTEXT:
        strcpy(err_str, "CL_INVALID_CONTEXT");
        break;
    case CL_INVALID_QUEUE_PROPERTIES:
        strcpy(err_str, "CL_INVALID_QUEUE_PROPERTIES");
        break;
    case CL_INVALID_COMMAND_QUEUE:
        strcpy(err_str, "CL_INVALID_COMMAND_QUEUE");
        break;
    case CL_INVALID_HOST_PTR:
        strcpy(err_str, "CL_INVALID_HOST_PTR");
        break;
    case CL_INVALID_MEM_OBJECT:
        strcpy(err_str, "CL_INVALID_MEM_OBJECT");
        break;
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
        strcpy(err_str, "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR");
        break;
    case CL_INVALID_IMAGE_SIZE:
        strcpy(err_str, "CL_INVALID_IMAGE_SIZE");
        break;
    case CL_INVALID_SAMPLER:
        strcpy(err_str, "CL_INVALID_SAMPLER");
        break;
    case CL_INVALID_BINARY:
        strcpy(err_str, "CL_INVALID_BINARY");
        break;
    case CL_INVALID_BUILD_OPTIONS:
        strcpy(err_str, "CL_INVALID_BUILD_OPTIONS");
        break;
    case CL_INVALID_PROGRAM:
        strcpy(err_str, "CL_INVALID_PROGRAM");
        break;
    case CL_INVALID_PROGRAM_EXECUTABLE:
        strcpy(err_str, "CL_INVALID_PROGRAM_EXECUTABLE");
        break;
    case CL_INVALID_KERNEL_NAME:
        strcpy(err_str, "CL_INVALID_KERNEL_NAME");
        break;
    case CL_INVALID_KERNEL_DEFINITION:
        strcpy(err_str, "CL_INVALID_KERNEL_DEFINITION");
        break;
    case CL_INVALID_KERNEL:
        strcpy(err_str, "CL_INVALID_KERNEL");
        break;
    case CL_INVALID_ARG_INDEX:
        strcpy(err_str, "CL_INVALID_ARG_INDEX");
        break;
    case CL_INVALID_ARG_VALUE:
        strcpy(err_str, "CL_INVALID_ARG_VALUE");
        break;
    case CL_INVALID_ARG_SIZE:
        strcpy(err_str, "CL_INVALID_ARG_SIZE");
        break;
    case CL_INVALID_KERNEL_ARGS:
        strcpy(err_str, "CL_INVALID_KERNEL_ARGS");
        break;
    case CL_INVALID_WORK_DIMENSION:
        strcpy(err_str, "CL_INVALID_WORK_DIMENSION");
        break;
    case CL_INVALID_WORK_GROUP_SIZE:
        strcpy(err_str, "CL_INVALID_WORK_GROUP_SIZE");
        break;
    case CL_INVALID_WORK_ITEM_SIZE:
        strcpy(err_str, "CL_INVALID_WORK_ITEM_SIZE");
        break;
    case CL_INVALID_GLOBAL_OFFSET:
        strcpy(err_str, "CL_INVALID_GLOBAL_OFFSET");
        break;
    case CL_INVALID_EVENT_WAIT_LIST:
        strcpy(err_str, "CL_INVALID_EVENT_WAIT_LIST");
        break;
    case CL_INVALID_EVENT:
        strcpy(err_str, "CL_INVALID_EVENT");
        break;
    case CL_INVALID_OPERATION:
        strcpy(err_str, "CL_INVALID_OPERATION");
        break;
    case CL_INVALID_GL_OBJECT:
        strcpy(err_str, "CL_INVALID_GL_OBJECT");
        break;
    case CL_INVALID_BUFFER_SIZE:
        strcpy(err_str, "CL_INVALID_BUFFER_SIZE");
        break;
    case CL_INVALID_MIP_LEVEL:
        strcpy(err_str, "CL_INVALID_MIP_LEVEL");
        break;
    case CL_INVALID_GLOBAL_WORK_SIZE:
        strcpy(err_str, "CL_INVALID_GLOBAL_WORK_SIZE");
        break;
    case CL_INVALID_PROPERTY:
        strcpy(err_str, "CL_INVALID_PROPERTY");
        break;
    case CL_INVALID_IMAGE_DESCRIPTOR:
        strcpy(err_str, "CL_INVALID_IMAGE_DESCRIPTOR");
        break;
    case CL_INVALID_COMPILER_OPTIONS:
        strcpy(err_str, "CL_INVALID_COMPILER_OPTIONS");
        break;
    case CL_INVALID_LINKER_OPTIONS:
        strcpy(err_str, "CL_INVALID_LINKER_OPTIONS");
        break;
    case CL_INVALID_DEVICE_PARTITION_COUNT:
        strcpy(err_str, "CL_INVALID_DEVICE_PARTITION_COUNT");
        break;
    default:
        sprintf(err_str, "%d", err_code);
        break;
    }
    return err_str;
}

void CL_CALLBACK context_error(const char *errinfo,
                               const void *private_info,
                               size_t cb,
                               void *user_data)
{
    cl_context context = *((cl_context*)user_data);
    printf("Context %p reported an error by the callback function:\n",
           context);
    printf("%s", errinfo);
}

void CL_CALLBACK event_status_change(cl_event event,
                                     cl_int event_command_exec_status,
                                     void *user_data)
{
    printf("\tEvent %p status changed:\n", event);
    if(event_command_exec_status == CL_SUBMITTED){
        printf("\t\tCL_SUBMITTED\n");
    }
    else if(event_command_exec_status == CL_RUNNING){
        printf("\t\tCL_RUNNING\n");
    }
    else if(event_command_exec_status == CL_COMPLETE){
        printf("\t\tCL_COMPLETE\n");
    }
    else{
        printf("\t\t%d\n", event_command_exec_status);        
    }
}

int main(int argc, char *argv[])
{
    unsigned int i;
    cl_uint num_entries = 0, num_platforms = 0;
    cl_platform_id *platforms = NULL;
    cl_int flag;
    cl_bool test_failed = CL_FALSE;

    // Get the platforms
    flag = clGetPlatformIDs(0, NULL, &num_platforms);
    if(flag != CL_SUCCESS){
        printf("Error getting number of platforms\n");
        printf("\t%s\n", OpenCLError(flag));
        return EXIT_FAILURE;
    }
    if(!num_platforms){
        printf("No OpenCL platforms found...\n");
        return EXIT_FAILURE;
    }
    printf("%u platforms found...\n", num_platforms);

    num_entries = num_platforms;
    platforms   = (cl_platform_id*)malloc(num_entries*sizeof(cl_platform_id));
    if(!platforms){
        printf("Failure allocating memory for the platforms\n");
        return EXIT_FAILURE;
    }

    flag = clGetPlatformIDs(num_entries, platforms, &num_platforms);
    if(flag != CL_SUCCESS){
        printf("Error getting platforms\n");
        printf("\t%s\n", OpenCLError(flag));
        free(platforms); platforms = NULL;
        return EXIT_FAILURE;
    }

    // Work on each platform separately
    for(i = 0; i < num_platforms; i++){
        size_t platform_name_size = 0;
        char *platform_name = NULL;

        printf("Platform %u...\n", i);
        flag = clGetPlatformInfo(platforms[i],
                                 CL_PLATFORM_NAME,
                                 0,
                                 NULL,
                                 &platform_name_size);
        if(flag != CL_SUCCESS){
            printf("Failure getting platform name size\n");
            printf("\t%s\n", OpenCLError(flag));
            test_failed = CL_TRUE;
            continue;
        }

        platform_name = (char*)malloc(platform_name_size);
        if(!platform_name){
            printf("Failure allocating memory for the platform name\n");
            test_failed = CL_TRUE;
            continue;
        }
        flag = clGetPlatformInfo(platforms[i],
                                 CL_PLATFORM_NAME,
                                 platform_name_size,
                                 platform_name,
                                 NULL);
        if(flag != CL_SUCCESS){
            printf("Failure getting platform name\n");
            printf("\t%s\n", OpenCLError(flag));
            test_failed = CL_TRUE;
            free(platform_name); platform_name = NULL;
            continue;
        }
        printf("\t%s\n", platform_name);
        free(platform_name); platform_name = NULL;

        // Create the devices
        num_entries = 0;
        cl_uint num_devices = 0;
        cl_device_id *devices = NULL;
        flag = clGetDeviceIDs(platforms[i],
                              CL_DEVICE_TYPE_ALL,
                              num_entries,
                              devices,
                              &num_devices);
        if(flag != CL_SUCCESS){
            printf("Failure getting number of devices\n");
            printf("\t%s\n", OpenCLError(flag));
            test_failed = CL_TRUE;
            continue;
        }
        if(!num_devices){
            printf("\tWithout devices.\n");
            // this is not a fail actually, just move to next platform
            continue;
        }

        num_entries = num_devices;
        devices = (cl_device_id*)malloc(num_entries * sizeof(cl_device_id));
        if(!devices){
            printf("Failure allocating memory for the devices\n");
            test_failed = CL_TRUE;
            continue;
        }
        flag = clGetDeviceIDs(platforms[i],
                              CL_DEVICE_TYPE_ALL,
                              num_entries,
                              devices,
                              &num_devices);
        if(flag != CL_SUCCESS){
            printf("Failure getting the devices\n");
            printf("\t%s\n", OpenCLError(flag));
            if(devices) free(devices); devices = NULL;
            test_failed = CL_TRUE;
            continue;
        }

        // Create a context
        cl_context_properties context_properties[3] = {
            CL_CONTEXT_PLATFORM,
            (cl_context_properties)platforms[i],
            0
        };
        cl_context context = clCreateContext(context_properties,
                                             num_devices,
                                             devices,
                                             &context_error,
                                             &context,
                                             &flag);
        if(flag != CL_SUCCESS) {
            printf("Error building context\n");
            printf("\t%s\n", OpenCLError(flag));
            free(devices); devices = NULL;
            test_failed = CL_TRUE;
            continue;
        }
        printf("\tBuilt context with %u devices!\n", num_devices);

        // Create the user event
        cl_event event;
        event = clCreateUserEvent(context,
                                  &flag);
        if(flag != CL_SUCCESS) {
            printf("Error creating the user event\n");
            printf("\t%s\n", OpenCLError(flag));
            test_failed = CL_TRUE;
            continue;
        }
        printf("\tBuilt user event!\n");

        flag = clSetEventCallback(event,
                                  CL_SUBMITTED,
                                  &event_status_change,
                                  NULL);
        if(flag != CL_SUCCESS){
            printf("Error setting the event CL_SUBMITTED status callback\n");
            printf("\t%s\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        flag = clSetEventCallback(event,
                                  CL_RUNNING,
                                  &event_status_change,
                                  NULL);
        if(flag != CL_SUCCESS){
            printf("Error setting the event CL_RUNNING status callback\n");
            printf("\t%s\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        flag = clSetEventCallback(event,
                                  CL_COMPLETE,
                                  &event_status_change,
                                  NULL);
        if(flag != CL_SUCCESS){
            printf("Error setting the event CL_COMPLETE status callback\n");
            printf("\t%s\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        printf("\tRegistered the callbacks!\n");

        // Print the event data
        printf("\t\tCL_EVENT_COMMAND_QUEUE: ");
        cl_command_queue command_queue_ret = NULL;
        flag = clGetEventInfo(event,
                              CL_EVENT_COMMAND_QUEUE,
                              sizeof(cl_command_queue),
                              &command_queue_ret,
                              NULL);
        if(flag != CL_SUCCESS){
            printf("FAIL (%s)\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else if(command_queue_ret == NULL){
            printf("OK\n");
        }
        else{
            printf("FAIL\n");
            test_failed = CL_TRUE;
        }
        printf("\t\tCL_EVENT_CONTEXT: ");
        cl_context context_ret = NULL;
        flag = clGetEventInfo(event,
                              CL_EVENT_CONTEXT,
                              sizeof(cl_context),
                              &context_ret,
                              NULL);
        if(flag != CL_SUCCESS){
            printf("FAIL (%s)\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else if(context_ret == context){
            printf("OK\n");
        }
        else{
            printf("FAIL\n");
            test_failed = CL_TRUE;
        }
        printf("\t\tCL_EVENT_COMMAND_TYPE: ");
        cl_command_type command_type_ret = NULL;
        flag = clGetEventInfo(event,
                              CL_EVENT_COMMAND_TYPE,
                              sizeof(cl_command_type),
                              &command_type_ret,
                              NULL);
        if(flag != CL_SUCCESS){
            printf("FAIL (%s)\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else if(command_type_ret == CL_COMMAND_USER){
            printf("OK\n");
        }
        else{
            printf("FAIL\n");
            test_failed = CL_TRUE;
        }
        printf("\t\tCL_EVENT_COMMAND_EXECUTION_STATUS: ");
        cl_int event_status_ret = NULL;
        flag = clGetEventInfo(event,
                              CL_EVENT_COMMAND_EXECUTION_STATUS,
                              sizeof(cl_int),
                              &event_status_ret,
                              NULL);
        if(flag != CL_SUCCESS){
            printf("FAIL (%s)\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else if(event_status_ret == CL_SUBMITTED){
            printf("OK\n");
        }
        else{
            printf("FAIL\n");
            test_failed = CL_TRUE;
        }
        printf("\t\tCL_EVENT_REFERENCE_COUNT: ");
        cl_uint ref_count = NULL;
        flag = clGetEventInfo(event,
                              CL_EVENT_REFERENCE_COUNT,
                              sizeof(cl_uint),
                              &ref_count,
                              NULL);
        if(flag != CL_SUCCESS){
            printf("FAIL (%s)\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else if(ref_count >= 1){
            printf("OK (%u)\n", ref_count);
        }
        else{
            printf("FAIL (%u)\n", ref_count);
            test_failed = CL_TRUE;
        }

        // Change the status
        printf("\tChanging to CL_COMPLETE...\n");
        flag = clSetUserEventStatus(event,
                                    CL_COMPLETE);
        if(flag != CL_SUCCESS){
            printf("\tFAIL (%s)\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else{
            printf("\tOK\n");
        }
        printf("\tCL_EVENT_COMMAND_EXECUTION_STATUS: ");
        flag = clGetEventInfo(event,
                              CL_EVENT_COMMAND_EXECUTION_STATUS,
                              sizeof(cl_int),
                              &event_status_ret,
                              NULL);
        if(flag != CL_SUCCESS){
            printf("FAIL (%s)\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else if(event_status_ret == CL_COMPLETE){
            printf("OK\n");
        }
        else{
            printf("FAIL (%d)\n", event_status_ret);
            test_failed = CL_TRUE;
        }
        printf("\tChanging to CL_OUT_OF_RESOURCES (%d)...\n",
               CL_OUT_OF_RESOURCES);
        flag = clSetUserEventStatus(event,
                                    CL_OUT_OF_RESOURCES);
        if(flag != CL_INVALID_OPERATION){
            printf("\tFAIL (%s)\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else{
            printf("\tOK\n");
        }
        printf("\tCL_EVENT_COMMAND_EXECUTION_STATUS: ");
        flag = clGetEventInfo(event,
                              CL_EVENT_COMMAND_EXECUTION_STATUS,
                              sizeof(cl_int),
                              &event_status_ret,
                              NULL);
        if(flag != CL_SUCCESS){
            printf("FAIL (%s)\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else if(event_status_ret == 0){
            printf("OK\n");
        }
        else{
            printf("FAIL (%d)\n", event_status_ret);
            test_failed = CL_TRUE;
        }

        flag = clReleaseEvent(event);
        if(flag != CL_SUCCESS) {
            printf("Error releasing the memory object\n");
            printf("\t%s\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else{
            printf("\tRemoved the user event.\n");
        }

        flag = clReleaseContext(context);
        if(flag != CL_SUCCESS) {
            printf("Error releasing context\n");
            printf("\t%s\n", OpenCLError(flag));
            test_failed = CL_TRUE;
        }
        else{
            printf("\tRemoved context.\n");
        }
        if(devices) free(devices); devices=NULL;
    }
    if(platforms) free(platforms); platforms=NULL;
    if(test_failed) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}