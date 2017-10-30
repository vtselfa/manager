#pragma once

#include <cstddef>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

#include "throw-with-trace.hpp"


class Point
{
	private:

	Point();

	public:

	size_t id;
	std::vector<double> values;

	Point(size_t _id, const std::vector<double> &_values) :
			id(_id), values(_values)
	{
		if (values.size() == 0)
			throw_with_trace(std::runtime_error("A point must have at least one value"));
	}

	// Euclidian distance between two points
	double distance(const Point &o) const;

	static auto create(size_t id, const std::vector<double>& values)
	{
		return std::make_shared<Point>(id, values);
	}
};
typedef std::shared_ptr<Point> point_ptr_t;
typedef std::vector<point_ptr_t> points_t;


// Type for storing two point ids and the distance between the two points
typedef
	std::pair
	<
		std::pair
		<
			const point_ptr_t,
			const point_ptr_t
		>,
		double
	> P2PDist;


class Cluster
{
	private:

	std::vector<double> centroid;
	std::set<point_ptr_t> points;

	public:

	size_t id;

	Cluster(size_t _id, const std::vector<double> &_centroid) : centroid(_centroid), id(_id)
	{
		if (centroid.size() == 0)
			throw_with_trace(std::runtime_error("A cluster cannot be created with an empty centroid"));
	}

	void set_centroid(const std::vector<double> &cent)
	{
		if (cent.size() == 0)
			throw_with_trace(std::runtime_error("A cluster centroid connot be empty"));
		if (cent.size() != centroid.size() && points.size() != 0)
			throw_with_trace(std::runtime_error("The size of the centroid of a cluster cannot be changed if the cluster is not empty"));
		this->centroid = cent;
	}

	void addPoint(const point_ptr_t p);
	void removePoint(const point_ptr_t p);
	void updateMeans();
	const std::vector<double>& getCentroid() const { return centroid; }
	const std::set<point_ptr_t>& getPoints() const { return points; }
	std::string to_string() const;

	bool disjoint(const Cluster &c) const;

	// Cluster to point distances

	// Vector of distances of all the points in the cluster to p, itself excluded, if it's in the cluster
	std::vector<P2PDist> pairwise_distance(const point_ptr_t p) const;
	// Distance to the centroid
	double centroid_distance(const Point &p) const;
	// Mean distance from p to all the points in the cluster, itself excluded, if it's in the cluster
	double mean_pairwise_distance(const point_ptr_t p) const;
	// Min distance from p to all the points in the cluster, itself excluded, if it's in the cluster
	double min_pairwise_distance(const point_ptr_t p) const;
	// Max distance from p to all the points in the cluster, itself excluded, if it's in the cluster
	double max_pairwise_distance(const point_ptr_t p) const;


	// Intercluster distance measurement

	// Distance from one centroid to the other
	double centroid_to_centroid_distance(const Cluster &c) const;
	// Distance between the most separated points, one from each cluster
	double farthest_points_distance(const Cluster &c) const;
	// Distance between the closest points, one from each cluster
	double closest_points_distance(const Cluster &c) const;


	// Cluster density measurements

	// Distances between all the pairs of points in the cluster
	std::vector<P2PDist> pairwise_distance() const;
	// Distance between the closest two points in the cluster
	double min_pairwise_distance() const;
	// Distance between the farthest two points in the cluster
	double max_pairwise_distance() const;
	// Mean distance between all pairs
	double mean_pairwise_distance() const;
	// Mean distance of all the points to the cluster centroid
	double mean_centroid_distance() const;
};
typedef std::vector<Cluster> clusters_t;


enum class EvalClusters
{
	dunn,
	silhouette
};
EvalClusters str_to_evalclusters(std::string str);


class KMeans
{
	protected:

	// Return the index of the nearest cluster
	static
	int nearestCluster(const std::vector<Cluster> &clusters, const Point &point);

	// Put an initial Point in them
	static
	void initClusters(size_t k, const points_t &points, std::vector<Cluster> &clusters);

	static
	void reinitCluster(const points_t &points, Cluster &c);

	// Computes the Silhouette index
	static
	double silhouette(const std::vector<Cluster> &clusters);

	static
	double dunn_index(const std::vector<Cluster> &clusters);

	public:

	// Clusterize
	static
	size_t clusterize(size_t k, const points_t &points, std::vector<Cluster> &clusters, size_t max_iter);

	static
	size_t clusterize_optimally(size_t max_k, const points_t &points, std::vector<Cluster> &clusters, size_t max_iter, enum EvalClusters eval_clusters);

	static
	std::string to_string(const std::vector<Cluster> &clusters);
};
