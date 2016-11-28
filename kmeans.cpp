#include <cassert>
#include <cmath>
#include <limits>

#include <fmt/format.h>

#include "kmeans.hpp"
#include "log.hpp"


using fmt::literals::operator""_format;


// Euclidian distance between two points
double Point::distance(const Point &o) const
{
	double sum = 0;
	assert(values.size() == o.values.size());
	for(size_t i = 0; i < values.size(); i++)
		sum += pow(values[i] - o.values[i], 2);
	return sqrt(sum);
}


void Cluster::addPoint(const Point *point)
{
	points[point->id] = point;
}


void Cluster::removePoint(int id_point)
{
	points.erase(id_point);
}


// Distance to the cluster centroid
double Cluster::distance(const Point &p) const
{
	assert(p.values.size() == centroid.size());

	double sum = 0;
	for(size_t i = 0; i < p.values.size(); i++)
		sum += pow(centroid[i] - p.values[i], 2);
	return sqrt(sum);
}


// Average distance to the points in the cluster
double Cluster::mean_distance(const Point &p) const
{
	assert(points.size() > 0);
	size_t n = points.count(p.id) ? points.size() - 1 : points.size(); // If the point is in the cluster we ignore it
	double sum = 0;
	if (n == 0) // This is the only point in the cluster
		return 0;
	for(const auto &item : points)
	{
		if (item.first == p.id)
			continue;
		sum += p.distance(*item.second);
	}
	return sum / n;
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


// Computes the global silhouette index, a value between -1 and 1, where the closer to 1, the best
double KMeans::silhouette(const std::vector<Cluster> &clusters)
{
	double result = 0; // Global silhouette index
	for (size_t k = 0; k < clusters.size(); k++)
	{
		double sk = 0; // Silhouette index for the cluster
		for (const auto &item : clusters[k].getPoints())
		{
			const auto &point = *item.second;
			double a = clusters[k].mean_distance(point);
			double b = std::numeric_limits<double>::max();
			for (size_t k2 = 0; k2 < clusters.size(); k2++)
			{
				if (k == k2) continue;
				b = std::min(b, clusters[k2].mean_distance(point));
			}
			double si = (b - a) / std::max(a, b); // Silhouette index for the point
			sk += si;
		}
		sk /= clusters[k].getPoints().size();
		result += sk;
	}
	result /= clusters.size();
	assert(result >= -1 && result <= 1);
	return result / clusters.size();
}


void KMeans::initClusters(size_t k, const std::vector<Point> &points, std::vector<Cluster> &clusters)
{
	clusters.clear();
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


size_t KMeans::clusterize_optimally(size_t max_k, const std::vector<Point> &points, std::vector<Cluster> &clusters, size_t max_iter)
{
	double best_result = -1;
	size_t best_k = 0;
	std::vector<Cluster> best_clusters;
	size_t best_iter = 0;

	for (size_t k = 2; k <= max_k; k++)
	{
		size_t iter = clusterize(k, points, clusters, max_iter);
		double result = silhouette(clusters);
		LOGDEB("k {} has a silhouette of {}"_format(k, result));
		if (result > best_result)
		{
			best_result = result;
			best_k = k;
			best_clusters = clusters;
			best_iter = iter;
		}
	}
	clusters = best_clusters;

	assert(best_result >= -1 && best_result <= 1);
	assert(best_k == clusters.size());

	return best_iter;
}
