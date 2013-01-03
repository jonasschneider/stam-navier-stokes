#include "solvercl.h"

#include <cstdlib>
#include <cstdio>
#include <string>

#include "file2string.h"

#define DATA_SIZE (1024)


static void check_error(cl_int error, char* name) {
  if (error != CL_SUCCESS) {
    fprintf(stderr, "Non-successful return code %d for %s.  Exiting.\n", error, name);
    exit(1);
  }
}

int printPlatforms()
{
  cl_uint i;
  cl_int err;

  // Discover the number of platforms:
  cl_uint nplatforms;
  err = clGetPlatformIDs(0, NULL, &nplatforms);
  check_error(err, "clGetPlatformIds");

  // Now ask OpenCL for the platform IDs:
  cl_platform_id* platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * nplatforms);
  err = clGetPlatformIDs(nplatforms, platforms, NULL);
  check_error(err, "clGetPlatformIds");

  // Ask OpenCL about each platform to understand the problem:
  char name[128];
  char vendor[128];
  char version[128];

  fprintf(stdout, "OpenCL reports %d platforms.\n\n", nplatforms);

  for (i = 0; i < nplatforms; i++) {
    err |= clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, 128, vendor, NULL);
    err |= clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 128, name, NULL);
    err |= clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, 128, version, NULL);
    check_error(err, "clGetPlatformInfo");

    fprintf(stdout, "Platform %d: %s %s %s\n", i, vendor, name, version);
  }

  free(platforms);
  return 0;
}

void SolverCL::init(size_t bufferSize) {
  printPlatforms();

  cl_uint numPlatforms = 2;
  cl_platform_id platforms[2];
  cl_uint platformCount;
  clGetPlatformIDs(numPlatforms, platforms, &platformCount);

  cl_uint platformIndex = 0;

  if (platformCount > 1) {
    platformIndex = platformCount - 1;
  }

  //issues with

  // Connect to a compute device
  //
  cl_uint deviceCount = 0;
  err = clGetDeviceIDs(platforms[platformIndex], CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &deviceCount);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to create a device group!\n");
    return;
  }
  
  // Get some device info
  //
  cl_uint computeUnits;
  clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &computeUnits, NULL);
  
  printf("Found %u devices with %u compute units\n", deviceCount, computeUnits);
  
  // Create a compute context
  //
  context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
  if (!context)
  {
    printf("Error: Failed to create a compute context!\n");
    return;
  }
  
  // Create a command commands
  //
  commands = clCreateCommandQueue(context, device_id, 0, &err);

  if (!commands) {
    printf("Error: Failed to create a command commands!\n");
    return;
  }
  
  // density
  
  std::string kernelSourceString = file2string("solver.cl");
  const char *kernelSource = kernelSourceString.c_str();
  
  // Create the compute program from the source buffer
  //
  density_program = clCreateProgramWithSource(context, 1, (const char**)&kernelSource, NULL, &err);
  
  if (!density_program) {
    printf("Error: Failed to create compute program!\n");
    return;
  }
  
  // Build the density program executable
  //
  err = clBuildProgram(density_program, 0, NULL, NULL, NULL, NULL);

  if (err != CL_SUCCESS) {
    size_t len;
    char buffer[2048];
    printf("Error: Failed to build program executable!\n");
    clGetProgramBuildInfo(density_program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    printf("%s\n", buffer);
    exit(1);
  }
  
  // Create the compute kernel in the program we wish to run
  //
  density_kernel = clCreateKernel(density_program, "stepDensity", &err);
  if (!density_kernel || err != CL_SUCCESS)
  {
    printf("Error: Failed to create compute kernel!\n");
    exit(1);
  }
  
  // velocity
  
  // Create the compute program from the source buffer
  //
  velocity_program = clCreateProgramWithSource(context, 1, (const char**)&kernelSource, NULL, &err);

  if (!velocity_program) {
    printf("Error: Failed to create compute program!\n");
    return;
  }
  
  // Build the velocity program executable
  //
  err = clBuildProgram(velocity_program, 0, NULL, NULL, NULL, NULL);
  
  if (err != CL_SUCCESS) {
    size_t len;
    char buffer[2048];
    printf("Error: Failed to build program executable!\n");
    clGetProgramBuildInfo(velocity_program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    printf("%s\n", buffer);
    exit(1);
  }
  
  // Create the compute kernel in the program we wish to run
  //
  velocity_kernel = clCreateKernel(velocity_program, "stepVelocity", &err);
  if (!velocity_kernel || err != CL_SUCCESS)
  {
    printf("Error: Failed to create compute kernel!\n");
    exit(1);
  }

  
  // Create the input and output arrays in device memory for our calculation
  //
  u_mem = clCreateBuffer(context,  CL_MEM_READ_WRITE,  sizeof(float) * bufferSize, NULL, NULL);
  v_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * bufferSize, NULL, NULL);
  
  u_prev_mem = clCreateBuffer(context,  CL_MEM_READ_WRITE,  sizeof(float) * bufferSize, NULL, NULL);
  v_prev_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * bufferSize, NULL, NULL);

  if (!u_mem || !v_mem || !u_prev_mem || !v_prev_mem) {
    printf("Error: Failed to allocate device memory!\n");
    exit(1);
  }

  err = clGetKernelWorkGroupInfo(density_kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
  if (err != CL_SUCCESS) {
    printf("Error: Failed to retrieve kernel work group info! %d\n", err);
    exit(1);
  }
  
//  size_t maxWorkGroupSize = 0;
//  
//  err = clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, NULL);
//  if (err != CL_SUCCESS) {
//    printf("Error: Failed to retrieve device work group info! %d\n", err);
//    exit(1);
//  }
}

void SolverCL::destroy() {
  clReleaseMemObject(u_mem);
  clReleaseMemObject(v_mem);
  clReleaseMemObject(u_prev_mem);
  clReleaseMemObject(v_prev_mem);
  
  clReleaseProgram(density_program);
  clReleaseProgram(velocity_program);
  
  clReleaseKernel(velocity_kernel);
  clReleaseKernel(density_kernel);
  clReleaseCommandQueue(commands);
  clReleaseContext(context);
}

void SolverCL::stepDensity(int NW, int NH, float* u, float* v, float* u_prev, float* v_prev, float diff, float dt, size_t bufferSize) {

  int err = clEnqueueWriteBuffer(commands, u_mem, CL_TRUE, 0, sizeof(float) * bufferSize, u, 0, NULL, NULL);
  if (err != CL_SUCCESS) {
    printf("Error: Failed to write to source array!\n");
    exit(1);
  }
  
  err = clEnqueueWriteBuffer(commands, v_mem, CL_TRUE, 0, sizeof(float) * bufferSize, v, 0, NULL, NULL);
  if (err != CL_SUCCESS) {
    printf("Error: Failed to write to source array!\n");
    exit(1);
  }

  err = clEnqueueWriteBuffer(commands, u_prev_mem, CL_TRUE, 0, sizeof(float) * bufferSize, u_prev, 0, NULL, NULL);
  if (err != CL_SUCCESS) {
    printf("Error: Failed to write to source array!\n");
    exit(1);
  }
  
  err = clEnqueueWriteBuffer(commands, v_prev_mem, CL_TRUE, 0, sizeof(float) * bufferSize, v_prev, 0, NULL, NULL);
  if (err != CL_SUCCESS) {
    printf("Error: Failed to write to source array!\n");
    exit(1);
  }

  // set arguments
  
  err = 0;
  err  = clSetKernelArg(density_kernel, 0, sizeof(unsigned int), &NW);
  err |= clSetKernelArg(density_kernel, 1, sizeof(unsigned int), &NH);
  err |= clSetKernelArg(density_kernel, 2, sizeof(cl_mem), &u_mem);
  err |= clSetKernelArg(density_kernel, 3, sizeof(cl_mem), &v_mem);
  err |= clSetKernelArg(density_kernel, 4, sizeof(cl_mem), &u_prev_mem);
  err |= clSetKernelArg(density_kernel, 5, sizeof(cl_mem), &v_prev_mem);
  err |= clSetKernelArg(density_kernel, 6, sizeof(float), &diff);
  err |= clSetKernelArg(density_kernel, 7, sizeof(float), &dt);
  
  if (err != CL_SUCCESS) {
    printf("Error: Failed to set kernel arguments! %d\n", err);
    exit(1);
  }
  
  // execute
  
  err = clGetKernelWorkGroupInfo(density_kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to retrieve kernel work group info! %d\n", err);
    exit(1);
  }

  global = 1;
  local = 1;
  err = clEnqueueNDRangeKernel(commands, density_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
  if (err) {
    printf("Error: Failed to execute kernel!\n");
    return;
  }
  
  // cleanup

  clFinish(commands);
  
  err = clEnqueueReadBuffer(commands, u_mem, CL_TRUE, 0, sizeof(float) * bufferSize, u, 0, NULL, NULL);
  
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output array! %d\n", err);
    exit(1);
  }
  
  err = clEnqueueReadBuffer(commands, v_mem, CL_TRUE, 0, sizeof(float) * bufferSize, v, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output array! %d\n", err);
    exit(1);
  }
  
  err = clEnqueueReadBuffer(commands, u_prev_mem, CL_TRUE, 0, sizeof(float) * bufferSize, u_prev, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output array! %d\n", err);
    exit(1);
  }
  
  err = clEnqueueReadBuffer(commands, v_prev_mem, CL_TRUE, 0, sizeof(float) * bufferSize, v_prev, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output array! %d\n", err);
    exit(1);
  }
  
  clFinish(commands);
}

void SolverCL::stepVelocity(int NW, int NH, float* u, float* v, float* u_prev, float* v_prev, float visc, float dt, size_t bufferSize) {
  int err = clEnqueueWriteBuffer(commands, u_mem, CL_TRUE, 0, sizeof(float) * bufferSize, u, 0, NULL, NULL);
  if (err != CL_SUCCESS) {
    printf("Error: Failed to write to source array!\n");
    exit(1);
  }
  
  err = clEnqueueWriteBuffer(commands, v_mem, CL_TRUE, 0, sizeof(float) * bufferSize, v, 0, NULL, NULL);
  if (err != CL_SUCCESS) {
    printf("Error: Failed to write to source array!\n");
    exit(1);
  }
  
  err = clEnqueueWriteBuffer(commands, u_prev_mem, CL_TRUE, 0, sizeof(float) * bufferSize, u_prev, 0, NULL, NULL);
  if (err != CL_SUCCESS) {
    printf("Error: Failed to write to source array!\n");
    exit(1);
  }
  
  err = clEnqueueWriteBuffer(commands, v_prev_mem, CL_TRUE, 0, sizeof(float) * bufferSize, v_prev, 0, NULL, NULL);
  if (err != CL_SUCCESS) {
    printf("Error: Failed to write to source array!\n");
    exit(1);
  }
  
  // set arguments
  
  err = 0;
  err  = clSetKernelArg(velocity_kernel, 0, sizeof(unsigned int), &NW);
  err |= clSetKernelArg(velocity_kernel, 1, sizeof(unsigned int), &NH);
  err |= clSetKernelArg(velocity_kernel, 2, sizeof(cl_mem), &u_mem);
  err |= clSetKernelArg(velocity_kernel, 3, sizeof(cl_mem), &v_mem);
  err |= clSetKernelArg(velocity_kernel, 4, sizeof(cl_mem), &u_prev_mem);
  err |= clSetKernelArg(velocity_kernel, 5, sizeof(cl_mem), &v_prev_mem);
  err |= clSetKernelArg(velocity_kernel, 6, sizeof(float), &visc);
  err |= clSetKernelArg(velocity_kernel, 7, sizeof(float), &dt);
  
  if (err != CL_SUCCESS) {
    printf("Error: Failed to set kernel arguments! %d\n", err);
    exit(1);
  }
  
  // execute
  
  err = clGetKernelWorkGroupInfo(velocity_kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to retrieve kernel work group info! %d\n", err);
    exit(1);
  }
  
  global = 1;
  local = 1;
  err = clEnqueueNDRangeKernel(commands, velocity_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
  if (err) {
    printf("Error: Failed to execute kernel!\n");
    return;
  }
  
  // cleanup
  
  clFinish(commands);
  
  err = clEnqueueReadBuffer(commands, u_mem, CL_TRUE, 0, sizeof(float) * bufferSize, u, 0, NULL, NULL);
  
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output array! %d\n", err);
    exit(1);
  }
  
  err = clEnqueueReadBuffer(commands, v_mem, CL_TRUE, 0, sizeof(float) * bufferSize, v, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output array! %d\n", err);
    exit(1);
  }
  
  err = clEnqueueReadBuffer(commands, u_prev_mem, CL_TRUE, 0, sizeof(float) * bufferSize, u_prev, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output array! %d\n", err);
    exit(1);
  }
  
  err = clEnqueueReadBuffer(commands, v_prev_mem, CL_TRUE, 0, sizeof(float) * bufferSize, v_prev, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output array! %d\n", err);
    exit(1);
  }
  
  clFinish(commands);  
}
