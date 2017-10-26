#include <cassert>
#include <cmath>
#include <limits>

#include <fmt/format.h>

#include "kmeans.hpp"
#include "log.hpp"


using fmt::literals::operator""_format;


EvalClusters str_to_evalclusters(std::string str)
{
	if (str == "dunn")
		return EvalClusters::dunn;
	if (str == "silhouette")
		return EvalClusters::silhouette;
	throw_with_trace(std::runtime_error("Invalid EvalClusters"));
}


double Point::distance(const Point &o) const
{
	double sum = 0;
	assert(values.size() == o.values.size());
	for(size_t i = 0; i < values.size(); i++)
		sum += pow(values[i] - o.values[i], 2);
	return sqrt(sum);
}


void Cluster::addPoint(const point_ptr_t p)
{
	if (p == NULL)
		throw_with_trace(std::runtime_error("Cannot add a NULL Point pointer to cluster"));
	if (points.count(p))
		throw_with_trace(std::runtime_error("The point with id {} alredy exists in the cluster"_format(p->id)));
	if (p->values.size() != centroid.size())
		throw_with_trace(std::runtime_error("The point with id {} is {}-dimensional, while the cluster is {}-dimensional"_format(p->id, p->values.size(), centroid.size())));
	points.insert(p);
}


void Cluster::removePoint(const point_ptr_t p)
{
	if (p == NULL)
		throw_with_trace(std::runtime_error("Cannot remove a NULL Point pointer from the cluster"));
	if (points.count(p) == 0)
		throw_with_trace(std::runtime_error("The point with id: {} is not in the cluster"_format(p->id)));
	points.erase(p);
}


bool Cluster::disjoint(const Cluster &c) const
{
	const Cluster *cs;
	const Cluster *cb;

	if (c.getPoints().size() > points.size())
	{
		cb = &c;
		cs = this;
	}
	else
	{
		cs = &c;
		cb = this;
	}

	for (const auto &p : cs->getPoints())
		if (cb->getPoints().count(p) != 0)
			return false;
	return true;
}


double Cluster::centroid_distance(const Point &p) const
{
	assert(p.values.size() == centroid.size());

	double sum = 0;
	for(size_t i = 0; i < p.values.size(); i++)
		sum += pow(centroid[i] - p.values[i], 2);
	return sqrt(sum);
}


std::vector<P2PDist> Cluster::pairwise_distance(const point_ptr_t p) const
{
	if (points.size() == 0)
		throw_with_trace(std::runtime_error("Cannot compute the pairwise distance if the cluster is empty"));
	auto result = std::vector<P2PDist>();
	for(const auto q : points)
	{
		// Ignore p if it's in the cluster
		if (p == q)
			continue;
		P2PDist p2pdist = std::make_pair(std::make_pair(p, q), p->distance(*q));
		result.push_back(p2pdist);
	}
	return result;
}


std::vector<P2PDist> Cluster::pairwise_distance() const
{
	if (points.size() == 0)
		throw std::runtime_error("Cannot compute the pairwise distance if the cluster is empty");

	auto result = std::vector<P2PDist>();
	for (auto it1 = points.begin(); it1 != points.end(); it1++)
	{
		const point_ptr_t pi = *it1;
		auto it2 = it1;
		it2++;
		for (; it2 != points.end(); it2++)
		{
			const point_ptr_t pj = *it2;
			P2PDist p2pdist = std::make_pair(
					std::make_pair(pi, pj),
					pi->distance(*pj));
			result.push_back(p2pdist);
		}

	}
	return result;
}


double Cluster::mean_pairwise_distance(const point_ptr_t p) const
{
	auto distances = pairwise_distance(p);
	double sum = 0;
	for (const auto &p2pdist : distances)
		sum += p2pdist.second;
	return sum / distances.size();
}


double Cluster::min_pairwise_distance(const point_ptr_t p) const
{
	double min = std::numeric_limits<double>().max();
	auto distances = pairwise_distance(p);
	for (const auto &p2pdist : distances)
		if (p2pdist.second < min)
			min = p2pdist.second;
	return min;
}


double Cluster::max_pairwise_distance(const point_ptr_t p) const
{
	double max = 0;
	auto distances = pairwise_distance(p);
	for (const auto &p2pdist : distances)
		if (p2pdist.second > max)
			max = p2pdist.second;
	return max;
}


double Cluster::mean_pairwise_distance() const
{
	auto distances = pairwise_distance();
	double sum = 0;
	for (const auto &p2pdist : distances)
		sum += p2pdist.second;
	return sum / distances.size();
}


double Cluster::min_pairwise_distance() const
{
	auto distances = pairwise_distance();
	double min = std::numeric_limits<double>().max();
	for (const auto &p2pdist : distances)
		if (p2pdist.second < min)
			min = p2pdist.second;
	return min;
}


double Cluster::max_pairwise_distance() const
{
	auto distances = pairwise_distance();
	double max = 0;
	for (const auto &p2pdist : distances)
		if (p2pdist.second > max)
			max = p2pdist.second;
	return max;
}


double Cluster::centroid_to_centroid_distance(const Cluster &c) const
{
	return Point(0, centroid).distance(Point(1, c.getCentroid()));
}


double Cluster::farthest_points_distance(const Cluster &c) const
{
	double max = 0;
	for (const point_ptr_t p : c.getPoints())
	{
		double local_max = max_pairwise_distance(p);
		if (local_max > max)
			max = local_max;
	}
	return max;
}


double Cluster::closest_points_distance(const Cluster &c) const
{
	double min = std::numeric_limits<double>().max();

	// If the clusters are not disjoint then the minimum distance is 0
	if (!disjoint(c))
		return 0;

	for (const point_ptr_t p : c.getPoints())
	{
		double local_min = min_pairwise_distance(p);
		if (local_min < min)
			min = local_min;
	}
	return min;
}


double Cluster::mean_centroid_distance() const
{
	auto center = Point(-1U, this->centroid);
	double sum = 0;
	for (const point_ptr_t p : points)
		sum += centroid_distance(*p);
	return sum / points.size();
}


void Cluster::updateMeans()
{
	assert(centroid.size() > 0);
	for(size_t i = 0; i < centroid.size(); i++)
	{
		double sum = 0;
		for(const auto &p : points)
			sum += p->values[i];
		centroid[i] = points.size() ? sum / points.size() : NAN;
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


int KMeans::nearestCluster(const std::vector<Cluster> &clusters, const Point &point)
{
	if (clusters.size() == 0)
		throw_with_trace(std::runtime_error("No clusters provided"));

	int cluster = 0;
	double min_dist = std::numeric_limits<double>::infinity();

	for (size_t i = 0; i < clusters.size(); i++)
	{
		double dist = clusters[i].centroid_distance(point);

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
		for (const point_ptr_t p : clusters[k].getPoints())
		{
			double a = clusters[k].mean_pairwise_distance(p);
			double b = std::numeric_limits<double>::max();
			// The pairwise distance can be NAN is this is the only member of the cluster.
			// If that is the case we set it to 0.
			if (std::isnan(a))
				a = 0;
			for (size_t k2 = 0; k2 < clusters.size(); k2++)
			{
				if (k == k2) continue;
				b = std::min(b, clusters[k2].mean_pairwise_distance(p));
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


double KMeans::dunn_index(const std::vector<Cluster> &clusters)
{
	double min_inter = std::numeric_limits<double>().max();
	for (size_t i = 0; i < clusters.size(); i++)
	{
		const auto &ci = clusters[i];
		for (size_t j = i + 1; j < clusters.size(); j++)
		{
			const auto &cj = clusters[j];
			double dist = ci.closest_points_distance(cj);
			if (dist < min_inter)
				min_inter = dist;
		}
	}
	double max_intra = 0;
	for (size_t i = 0; i < clusters.size(); i++)
	{
		double dist = clusters[i].max_pairwise_distance();
		if (dist > max_intra)
			max_intra = dist;
	}
	return -min_inter / max_intra; // Dunn index is the lower, the better, but we want the opposite

}


void KMeans::initClusters(size_t k, const points_t &points, std::vector<Cluster> &clusters)
{
	clusters.clear();
	double dist = (double) points.size() / (double) k;
	for (size_t i = 0 ; i < k ; i++)
	{
		size_t index = round(dist * i);
		clusters.push_back(Cluster(i, points[index]->values));
	}
}


void KMeans::reinitCluster(const points_t &points, Cluster &c)
{
	size_t index = round((double) rand() / (double) RAND_MAX * (double) (points.size() - 1));
	c.set_centroid(points[index]->values);
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


size_t KMeans::clusterize(size_t k, const points_t &points, std::vector<Cluster> &clusters, size_t max_iter)
{
	assert(points.size() > 0);

	const size_t total_points = points.size();

	if (total_points < k)
	{
		LOGWAR("There are less points ({}) than desired clusters ({}), so each point will go to a different cluster"_format(total_points, k));
		k = total_points;
	}

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
			int id_nearest_center = nearestCluster(clusters, *points[i]);

			if(id_old_cluster != id_nearest_center)
			{
				if(id_old_cluster != -1)
					clusters[id_old_cluster].removePoint(points[i]);

				assigned[i] = id_nearest_center;
				clusters[id_nearest_center].addPoint(points[i]);
				done = false;
			}
		}

		// Recalculate centroids
		for(size_t i = 0; i < k; i++)
		{
			if (clusters[i].getPoints().size() == 0)
			{
				reinitCluster(points, clusters[i]);
				done = false;
			}
			else
				clusters[i].updateMeans();
		}

		if(done == true || iter >= max_iter)
			break;
	}

	return iter;
}


size_t KMeans::clusterize_optimally(size_t max_k, const points_t &points, std::vector<Cluster> &clusters, size_t max_iter, enum EvalClusters eval_clusters)
{
	double best_result = -std::numeric_limits<double>().infinity();
	size_t best_k = -1U;
	std::vector<Cluster> best_clusters;
	size_t best_iter = 0;

	for (size_t k = 2; k <= std::min(max_k, points.size() - 1); k++)
	{
		size_t iter = clusterize(k, points, clusters, max_iter);
		double result;
		if (eval_clusters == EvalClusters::dunn)
			result = dunn_index(clusters);
		else if (eval_clusters == EvalClusters::silhouette)
			result = silhouette(clusters);
		else
			throw_with_trace(std::runtime_error("Unknown eval_clusters algorithm"));
		LOGDEB("k {} has a score of {}"_format(k, result));
		if (result > best_result)
		{
			best_result = result;
			best_k = k;
			best_clusters = clusters;
			best_iter = iter;
		}
	}
	clusters = best_clusters;

	assert(best_k == clusters.size());
	assert(clusters.size() > 0);
	assert(clusters.size() <= max_k);

	return best_iter;
}
