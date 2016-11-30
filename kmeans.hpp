#pragma once

#include <unordered_map>
#include <vector>


class Point
{
	public:

	const size_t id;
	const std::vector<double> values;

	Point(size_t id, const std::vector<double>& values) :
			id(id), values(values) {}

	// Euclidian distance between two points
	double distance(const Point &o) const;
};


// Type for storing two point ids and the distance between the two points
typedef
	std::pair
	<
		std::pair
		<
			size_t,
			size_t
		>,
		double
	> P2PDist;


class Cluster
{
	private:

	std::vector<double> centroid;
	std::unordered_map<size_t, const Point *> points;

	public:

	size_t id;

	Cluster(size_t id, const std::vector<double> &centroid) : centroid(centroid), id(id) {}

	void addPoint(const Point *point);
	void removePoint(size_t id_point);
	void updateMeans();
	const std::vector<double>& getCentroid() const { return centroid; }
	const auto& getPoints() const { return points; }
	std::string to_string() const;


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
	private:

	// Return the index of the nearest cluster
	static
	int nearestCluster(size_t k, const std::vector<Cluster> &clusters, const Point &point);

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
