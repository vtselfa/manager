#include <cassert>
#include <cmath>
#include <limits>

#include <fmt/format.h>

#include "kmeans.hpp"


void Cluster::addPoint(const Point *point)
{
	points[point->id] = point;
}


void Cluster::removePoint(int id_point)
{
	points.erase(id_point);
}


double Cluster::distance(const Point &p) const
{
	assert(p.values.size() == centroid.size());

	double sum = 0;
	for(size_t i = 0; i < p.values.size(); i++)
		sum += pow(centroid[i] - p.values[i], 2);
	return sqrt(sum);
}


void Cluster::updateMeans()
{
	if (centroid.size() == 0)
		return;
	for(size_t i = 0; i < centroid.size(); i++)
	{
		double sum = 0;
		for(const auto &item : points)
			sum += item.second->values[i];
		centroid[i] = sum / points.size();
	}
}


std::string Cluster::to_string() const
{
	size_t i;
	std::string values_str;
	for (i = 0; i < centroid.size() - 1; i++)
		values_str += fmt::format("{:.3g}", centroid[i]) + ", ";
	values_str += fmt::format("{:g}", centroid[i]);
	return fmt::format("{{id: {}, values: [{}], num_points: {}}}", id, values_str, points.size());
}


int KMeans::nearestCluster(size_t k, const std::vector<Cluster> &clusters, const Point &point)
{
	assert(clusters.size() > 0);

	int cluster = 0;
	double min_dist = std::numeric_limits<double>::infinity();

	for (size_t i = 0; i < k; i++)
	{
		double dist = clusters[i].distance(point);

		if (dist < min_dist)
		{
			min_dist = dist;
			cluster = i;
		}
	}

	return cluster;
}


void KMeans::initClusters(size_t k, const std::vector<Point> &points, std::vector<Cluster> &clusters)
{
	double dist = (double) points.size() / (double) k;
	for (size_t i = 0 ; i < k ; i++)
	{
		size_t index = round(dist * i);
		clusters.push_back(Cluster(i, points[index].values));
	}
}


std::string KMeans::to_string(const std::vector<Cluster> &clusters)
{
	std::string result = "[";
	size_t i;
	for (i = 0; i < clusters.size() - 1; i++)
		result += clusters[i].to_string() + ", ";
	result += clusters[i].to_string() + "]";
	return result;
}


size_t KMeans::clusterize(size_t k, const std::vector<Point> &points, std::vector<Cluster> &clusters, size_t max_iter)
{
	assert(points.size() > 0);

	const size_t total_points = points.size();

	assert(k <= total_points);

	// Choose k distinct values for the centers of the clusters
	initClusters(k, points, clusters);

	// Vector with the cluster each point belongs to
	auto assigned = std::vector<int>(points.size(), -1);

	size_t iter;
	for (iter = 0; iter < max_iter; iter++)
	{
		bool done = true;

		// Associate each point to the nearest cluster
		for(size_t i = 0; i < total_points; i++)
		{
			int id_old_cluster = assigned[i];
			int id_nearest_center = nearestCluster(k, clusters, points[i]);

			if(id_old_cluster != id_nearest_center)
			{
				if(id_old_cluster != -1)
					clusters[id_old_cluster].removePoint(points[i].id);

				assigned[i] = id_nearest_center;
				clusters[id_nearest_center].addPoint(&points[i]);
				done = false;
			}
		}

		// Recalculate centroids
		for(size_t i = 0; i < k; i++)
			clusters[i].updateMeans();

		if(done == true || iter >= max_iter)
			break;
	}

	return iter;
}
