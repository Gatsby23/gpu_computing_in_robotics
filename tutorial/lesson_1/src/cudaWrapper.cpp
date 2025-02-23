#include "cudaWrapper.h"


CCudaWrapper::CCudaWrapper()
{

}

CCudaWrapper::~CCudaWrapper()
{

}

void CCudaWrapper::warmUpGPU()
{
	cudaError_t err = ::cudaSuccess;
	err = cudaSetDevice(0);
		if(err != ::cudaSuccess)return;

	err = cudaWarmUpGPU();
		if(err != ::cudaSuccess)return;

}

int CCudaWrapper::getNumberOfAvailableThreads()
{
	cudaDeviceProp prop;
	cudaGetDeviceProperties(&prop,0);

	int threads = 0;
	if(prop.major == 2)
	{
		threads=prop.maxThreadsPerBlock/2;
	}else if(prop.major > 2)
	{
		threads=prop.maxThreadsPerBlock;
	}else
	{
		return 0;
	}

	return threads;
}

void CCudaWrapper::coutMemoryStatus()
{
	size_t free_byte ;
    size_t total_byte ;

    cudaError_t err = cudaMemGetInfo( &free_byte, &total_byte ) ;

    if(err != ::cudaSuccess)
	{
		std::cout << "Error: cudaMemGetInfo fails: " << cudaGetErrorString(err) << std::endl;
		return;
	}
    double free_db = (double)free_byte ;
    double total_db = (double)total_byte ;
    double used_db = total_db - free_db ;

    std::cout << "GPU memory usage: used = " <<
    		used_db/1024.0/1024.0 <<
			"(MB), free = " <<
			free_db/1024.0/1024.0 <<
			"(MB), total = " <<
			total_db/1024.0/1024.0 <<
			"(MB)" << std::endl;
}


bool CCudaWrapper::downsampling(pcl::PointCloud<pcl::PointXYZ> &point_cloud, float resolution)
{
	cudaError_t err = ::cudaSuccess;
	err = cudaSetDevice(0);
    if(err != ::cudaSuccess)return false;
    // The parameters used for the downsampling.
	gridParameters rgd_params;
	pcl::PointXYZ * d_point_cloud;
	hashElement* d_hashTable = NULL;
	bucket* d_buckets = NULL;
	bool* d_markers;
	bool* h_markers;
	int threads = getNumberOfAvailableThreads();

	std::cout << "CUDA code will use " << threads << " device threads" << std::endl;
	if(threads == 0)return false;

    // Allocate the memory on the GPU device.
	err = cudaMalloc((void**)&d_point_cloud, point_cloud.points.size()*sizeof(pcl::PointXYZ) );
    if(err != ::cudaSuccess)return false;
    // Copy the point data from the CPU to the GPU.
	err = cudaMemcpy(d_point_cloud, point_cloud.points.data(), point_cloud.points.size()*sizeof(pcl::PointXYZ), cudaMemcpyHostToDevice);
    if(err != ::cudaSuccess)return false;

    // Get the corresponding parameters.
	err = cudaCalculateGridParams(d_point_cloud, point_cloud.points.size(),
			resolution, resolution, resolution, rgd_params);
    if(err != ::cudaSuccess)return false;

    std::cout << "regular grid parameters:" << std::endl;
    std::cout << "bounding_box_min_X: " << rgd_params.bounding_box_min_X << std::endl;
    std::cout << "bounding_box_min_Y: " << rgd_params.bounding_box_min_Y << std::endl;
    std::cout << "bounding_box_min_Z: " << rgd_params.bounding_box_min_Z << std::endl;
    std::cout << "bounding_box_max_X: " << rgd_params.bounding_box_max_X << std::endl;
    std::cout << "bounding_box_max_Y: " << rgd_params.bounding_box_max_Y << std::endl;
    std::cout << "bounding_box_max_Z: " << rgd_params.bounding_box_max_Z << std::endl;
    std::cout << "number_of_buckets_X: " << rgd_params.number_of_buckets_X << std::endl;
    std::cout << "number_of_buckets_Y: " << rgd_params.number_of_buckets_Y << std::endl;
    std::cout << "number_of_buckets_Z: " << rgd_params.number_of_buckets_Z << std::endl;
    std::cout << "resolution_X: " << rgd_params.resolution_X << std::endl;
    std::cout << "resolution_Y: " << rgd_params.resolution_Y << std::endl;
    std::cout << "resolution_Z: " << rgd_params.resolution_Z << std::endl;
    // Allocate the memory on the GPU device.
	err = cudaMalloc((void**)&d_hashTable,point_cloud.points.size()*sizeof(hashElement));
			if(err != ::cudaSuccess)return false;
	err = cudaMalloc((void**)&d_buckets, rgd_params.number_of_buckets*sizeof(bucket));
			if(err != ::cudaSuccess)return false;
    // Calculate the grid of the point cloud.
	err = cudaCalculateGrid(threads, d_point_cloud, d_buckets, d_hashTable, point_cloud.points.size(), rgd_params);
			if(err != ::cudaSuccess)return false;

	err = cudaMalloc((void**)&d_markers, point_cloud.points.size()*sizeof(bool) );
			if(err != ::cudaSuccess)return false;

	err = cudaDownSample(threads, d_markers, d_hashTable, d_buckets, rgd_params, point_cloud.points.size());
			if(err != ::cudaSuccess)return false;

	h_markers = (bool *)malloc(point_cloud.points.size()*sizeof(bool));

	err = cudaMemcpy(h_markers, d_markers, point_cloud.points.size()*sizeof(bool),cudaMemcpyDeviceToHost);
			if(err != ::cudaSuccess)return false;

	pcl::PointCloud<pcl::PointXYZ> downsampled_point_cloud;
	for(size_t i = 0; i < point_cloud.points.size(); i++)
	{
		if(h_markers[i])downsampled_point_cloud.push_back(point_cloud[i]);
	}


	std::cout << "Number of points before down-sampling: " << point_cloud.size() << std::endl;

	point_cloud = downsampled_point_cloud;
	std::cout << "Number of points after down-sampling: " << point_cloud.size() << std::endl;

	std::cout << "Before cudaFree" << std::endl;
	coutMemoryStatus();

	free(h_markers);

	err = cudaFree(d_point_cloud); d_point_cloud = NULL;
			if(err != ::cudaSuccess)return false;

	err = cudaFree(d_hashTable); d_hashTable = NULL;
			if(err != ::cudaSuccess)return false;

	err = cudaFree(d_buckets); d_buckets = NULL;
		if(err != ::cudaSuccess)return false;

	err = cudaFree(d_markers); d_markers = NULL;
			if(err != ::cudaSuccess)return false;

	std::cout << "After cudaFree" << std::endl;
	coutMemoryStatus();

	return true;
}
