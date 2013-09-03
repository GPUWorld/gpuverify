#include <map>
#include <vector>
#include <iostream>
#include <string.h>
#include <stdio.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

struct arg_data {
	size_t size;
	void* data;
};

struct kernel_data {
	cl_program program;
	std::string name;
	std::vector<struct arg_data> args;
	unsigned int dimension;
	size_t global_size [3];
	size_t local_size [3];
};

class Col_Logger {
	public:
		std::map<cl_program, std::vector<std::string> > programs;
		std::map<cl_program, std::string> options;
		std::map<cl_kernel, struct kernel_data> kernels;

		void dump(cl_kernel karnol)
		{
			struct kernel_data kernel = kernels[karnol];
			std::cerr << "GPUVerify args:" << std::endl;
			std::cerr << " --local_size=[";
			std::cerr << kernel.local_size[0];
			if (kernel.dimension > 1)
				for (int i = 1; i < kernel.dimension; i++)
					std::cerr << "," << kernel.local_size[i];
			std::cerr << "]";

			std::cerr << " --num_groups=[";
			std::cerr << kernel.global_size[0]/kernel.local_size[0];
			if (kernel.dimension > 1)
				for (int i = 1; i < kernel.dimension; i++)
					std::cerr << "," << kernel.global_size[i]/kernel.local_size[i];
			std::cerr << "]";

			std::cerr << " --params=[" << kernel.name;
			for (int i = 0; i < kernel.args.size(); i++)
			{
				std::cerr << ",";
				if (kernel.args[i].data != NULL)
					for (int j = kernel.args[i].size - 1; j >= 0 ; j--)
						fprintf(stderr, "%02X", ((unsigned char*) kernel.args[i].data)[j]);
				else
					fprintf(stderr, "%hhX", 0);
			}
			std::cerr << "]";

			std::cerr << std::endl;

			std::cerr << "options:" << std::endl;
			std::cerr << options[kernel.program];
			std::cerr << std::endl;

			std::cerr << "code:" << std::endl;
			std::vector<std::string> code = programs[kernel.program];
			for (int i = 0; i < code.size(); i++)
				std::cerr << code[i];
		}

		void dump(void)
		{
			std::cerr << "DUMPING!" << std::endl;

			std::map<cl_kernel, struct kernel_data>::iterator it;
			for (it = kernels.begin(); it != kernels.end(); ++it)
			{
				this->dump(it->first);
			}
		}

		void clear (void)
		{
			programs.clear();
			kernels.clear();
		}

		/*
		~Col_Logger(void)
		{
			this->dump();
		}
		*/
};

Col_Logger& singleton(void)
{
	static Col_Logger t;
	return t;
}

extern "C" {

cl_mem clCreateBuffer_hook (cl_context context,
                            cl_mem_flags flags,
                            size_t size,
                            void *host_ptr,
                            cl_int *errcode_ret)
{
	cl_mem mem = clCreateBuffer(context, flags, size, host_ptr, errcode_ret);
	/*
	struct mem_data data;
	data.size = size;
	data.flags = flags;
	mem_datas[mem] = data;
	*/
  return mem;
}

cl_program clCreateProgramWithSource_hook (cl_context context,
                                           cl_uint count,
                                           const char **strings,
                                           const size_t *lengths,
                                           cl_int *errcode_ret)
{
	cl_program program = clCreateProgramWithSource(context, count, strings, lengths, errcode_ret);
	std::vector<std::string> code (count);
	if (lengths == NULL)
		for (int i = 0; i < count; i++)
		{
			std::string line (strings[i]);
			code.push_back(line);
		}
	else
		for (int i = 0; i < count; i++)
		{
			if (lengths[i] == 0) {
				std::string line (strings[i]);
				code.push_back(line);
			}
			else {
				std::string line (strings[i],lengths[i]);
				code.push_back(line);
			}
		}
	singleton().programs[program] = code;

  return program;
}

cl_int clBuildProgram_hook (cl_program program,
                            cl_uint num_devices,
                            const cl_device_id *device_list,
                            const char *options,
                            void (*pfn_notify)(cl_program, void *user_data),
                            void *user_data)
{
	std::string opts (options);
	singleton().options[program] = opts;
  return clBuildProgram(program, num_devices, device_list, options, pfn_notify, user_data);
}

cl_kernel clCreateKernel_hook (cl_program  program,
                               const char *kernel_name,
                               cl_int *errcode_ret)
{
	cl_kernel kernel = clCreateKernel(program, kernel_name, errcode_ret);
	struct kernel_data data;
	data.program = program;
	data.name = std::string (kernel_name);
	singleton().kernels[kernel] = data;
  return kernel;
}

cl_int clSetKernelArg_hook (cl_kernel kernel,
                            cl_uint arg_index,
                            size_t arg_size,
                            const void *arg_value)
{
	cl_int ret = clSetKernelArg(kernel, arg_index, arg_size, arg_value);
	if (singleton().kernels[kernel].args.size() < arg_index+1)
		singleton().kernels[kernel].args.resize(arg_index + 1);
	singleton().kernels[kernel].args[arg_index].size = arg_size;
	if (arg_value)
	{
		singleton().kernels[kernel].args[arg_index].data = malloc(arg_size);
		memcpy(singleton().kernels[kernel].args[arg_index].data, arg_value, arg_size);
	}
	else
		singleton().kernels[kernel].args[arg_index].data = NULL;
  return ret;
}

cl_int clEnqueueNDRangeKernel_hook (cl_command_queue command_queue,
                                    cl_kernel kernel,
                                    cl_uint work_dim,
                                    const size_t *global_work_offset,
                                    const size_t *global_work_size,
                                    const size_t *local_work_size,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event)

{
	cl_int ret = clEnqueueNDRangeKernel(command_queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event);
	singleton().kernels[kernel].dimension = work_dim;
	for (int i = 0; i < work_dim; i++)
	{
		singleton().kernels[kernel].global_size[i] = global_work_size[i];
		singleton().kernels[kernel].local_size[i] = local_work_size[i];
	}
	singleton().dump(kernel);
	//singleton().clear(); // Comment out this line if you want the kernel source/name re-dumped over and over.
  return ret;
}

}