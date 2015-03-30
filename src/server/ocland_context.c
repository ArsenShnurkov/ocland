/*
 *  This file is part of ocland, a free cloud OpenCL interface.
 *  Copyright (C) 2015  Jose Luis Cercos Pita <jl.cercos@upm.es>
 *
 *  ocland is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ocland is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with ocland.  If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 * @brief cl_context management
 * @see ocland_context.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include <ocland/common/sockets.h>
#include <ocland/common/dataExchange.h>
#include <ocland/common/dataPack.h>
#include <ocland/common/verbose.h>
#include <ocland/server/ocland_context.h>

/** @brief Callback function to be registered by the contexts.
 * @param errinfo Error description generated by OpenCL.
 * @param private_info Private data generated by OpenCL.
 * @param cb Size of \a private_info.
 * @param user_data Context object.
 */
void (CL_CALLBACK context_notify)(const char  *errinfo,
  	                              const void  *private_info,
                                  size_t       cb,
  	                              void        *user_data)
{
    int socket_flag = 0;
    ocland_context context = (ocland_context)user_data;
    int* sockfd = context->sockcb;
    void* identifier = (void*)(context->identifier);

    size_t ret_cb = (strlen(errinfo) + 1) * sizeof(char) + cb;
    void *ret_info = NULL;
    if(ret_cb){
        ret_info = malloc(ret_cb);
        if(!ret_info){
            // FIXME Memory fail, how to proceed??
            VERBOSE(
                "Memory allocation failure (%lu bytes) in context_notify.\n",
                ret_cb);
            VERBOSE("\terrinfo = \"%s\".\n", errinfo);
            return;
        }
        void* ptr = ret_info;
        memcpy(ptr, errinfo, (strlen(errinfo) + 1) * sizeof(char));
        ptr = (void*)((char *)ptr + strlen(errinfo) + 1);
        memcpy(ptr, private_info, cb);
    }
    // Call the client
    socket_flag |= Send(sockfd, &identifier, sizeof(void*), MSG_MORE);
    if(ret_cb){
        socket_flag |= Send(sockfd, &ret_cb, sizeof(size_t), MSG_MORE);
        socket_flag |= Send(sockfd, ret_info, ret_cb, 0);
    }
    else{
        socket_flag |= Send(sockfd, &ret_cb, sizeof(size_t), 0);
    }

    if(socket_flag < 0){
        VERBOSE("Communication failure during context_notify.\n");
        VERBOSE("\terrinfo = \"%s\".\n", errinfo);
        // FIXME Communication fail, how to proceed??
        return;
    }
}

ocland_context oclandCreateContext(cl_context_properties *properties,
                                   cl_uint num_devices,
                                   cl_device_id* devices,
                                   cl_context identifier,
                                   int *socket_cb,
                                   cl_int *errcode_ret)
{
    cl_int flag;
    // Generate the object
    ocland_context context = (ocland_context)malloc(
        sizeof(struct _ocland_context));
    if(!context){
        if(errcode_ret) *errcode_ret = CL_OUT_OF_RESOURCES;
        return NULL;
    }
    context->rcount = 1;
    context->sockcb = socket_cb;
    context->identifier = identifier;

    // Call OpenCL to generate the internal context
    void (CL_CALLBACK *pfn_notify)(const char*, const void *, size_t, void*);
    pfn_notify = NULL;
    void *user_data = NULL;
    if(identifier){
        pfn_notify = &context_notify;
        user_data = (void*)context;
    }
    context->context = clCreateContext(properties,
                                       num_devices,
                                       devices,
                                       pfn_notify,
                                       user_data,
                                       &flag);
    if(errcode_ret) *errcode_ret = flag;
    if(flag != CL_SUCCESS){
        return NULL;
    }
    return context;
}

ocland_context oclandCreateContextFromType(cl_context_properties *properties,
                                           cl_device_type device_type,
                                           cl_context identifier,
                                           int *socket_cb,
                                           cl_int *errcode_ret)
{
    cl_int flag;
    // Generate the object
    ocland_context context = (ocland_context)malloc(
        sizeof(struct _ocland_context));
    if(!context){
        if(errcode_ret) *errcode_ret = CL_OUT_OF_RESOURCES;
        return NULL;
    }
    context->rcount = 1;
    context->sockcb = socket_cb;
    context->identifier = identifier;

    // Call OpenCL to generate the internal context
    void (CL_CALLBACK *pfn_notify)(const char*, const void *, size_t, void*);
    pfn_notify = NULL;
    void *user_data = NULL;
    if(identifier){
        pfn_notify = &context_notify;
        user_data = (void*)context;
    }
    context->context = clCreateContextFromType(properties,
                                               device_type,
                                               pfn_notify,
                                               user_data,
                                               &flag);
    if(errcode_ret) *errcode_ret = flag;
    if(flag != CL_SUCCESS){
        return NULL;
    }
    return context;
}

cl_int oclandRetainContext(ocland_context context)
{
    cl_int flag = clRetainContext(context->context);
    if(flag != CL_SUCCESS){
        return flag;
    }

    context->rcount++;
    return CL_SUCCESS;
}

cl_int oclandReleaseContext(ocland_context context)
{
    cl_int flag = clReleaseContext(context->context);
    if(flag != CL_SUCCESS){
        return flag;
    }

    context->rcount--;
    if(!context->rcount){
        free(context);
    }
    return CL_SUCCESS;
}

cl_int oclandGetContextInfo(ocland_context   context,
                            cl_context_info  param_name,
  	                        size_t           param_value_size,
  	                        void            *param_value,
  	                        size_t          *param_value_size_ret)
{
    return clGetContextInfo(context->context,
                            param_name,
                            param_value_size,
                            param_value,
                            param_value_size_ret);
}
