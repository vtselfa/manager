#pragma once

#include <cstddef>
#include <set>
#include <vector>
#include <stdexcept>


class Point
{
	private:

		Point();

	public:

	const size_t id;
	const std::vector<double> values;

	Point(size_t id, const std::vector<double>& values) :
			id(id), values(values)
	{
		if (values.size() == 0)
			throw std::runtime_error("A point must have at least one value");
	}

	// Euclidian distance between two points
	double distance(const Point &o) const;
};


// Type for storing two point ids and the distance between the two points
typedef
	std::pair
	<
		std::pair
		<
			const Point *,
			const Point *
		>,
		double
	> P2PDist;


class Cluster
{
	private:

	std::vector<double> centroid;
	std::set<const Point *> points;

	public:

	size_t id;

	Cluster(size_t id, const std::vector<double> &centroid) : centroid(centroid), id(id)
	{
		if (centroid.size() == 0)
			throw std::runtime_error("A cluster cannot be created with an empty centroid");
	}

	void addPoint(const Point *p);
	void removePoint(const Point *p);
	void updateMeans();
	const std::vector<double>& getCentroid() const { return centroid; }
	const auto& getPoints() const { return points; }
	std::string to_string() const;

	bool disjoint(const Cluster &c) const;

	// Cluster to point distances

	// Vector of distances of all the points in the cluster to p, itself excluded, if it's in the cluster
	std::vector<P2PDist> pairwise_distance(const Point &p) const;
	// Distance to the centroid
	double centroid_distance(const Point &p) const;
	// Mean distance from p to all the points in the cluster, itself excluded, if it's in the cluster
	double mean_pairwise_distance(const Point &p) const;
	// Min distance from p to all the points in the cluster, itself excluded, if it's in the cluster
	double min_pairwise_distance(const Point &p) const;
	// Max distance from p to all the points in the cluster, itself excluded, if it's in the cluster
	double max_pairwise_distance(const Point &p) const;


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


class KMeans
{
	protected:

	// Return the index of the nearest cluster
	static
	int nearestCluster(const std::vector<Cluster> &clusters, const Point &point);

	// Put an initial Point in them
	static
	void initClusters(size_t k, const std::vector<Point> &points, std::vector<Cluster> &clusters);

	// Computes the Silhouette index
	static
	double silhouette(const std::vector<Cluster> &clusters);

	public:

	// Clusterize
	static
	size_t clusterize(size_t k, const std::vector<Point> &points, std::vector<Cluster> &clusters, size_t max_iter);

	static
	size_t clusterize_optimally(size_t max_k, const std::vector<Point> &points, std::vector<Cluster> &clusters, size_t max_iter);

	static
	std::string to_string(const std::vector<Cluster> &clusters);
};
