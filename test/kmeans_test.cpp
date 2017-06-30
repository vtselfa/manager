#include <iostream>
#include <cmath>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "kmeans.hpp"


class PointTest : public testing::Test
{};

TEST_F(PointTest, Constructor)
{
	const size_t id = 23;
	const std::vector<double> values = {1, 1.1, 2.3, -0.8};
	auto p = Point(id, values);

	EXPECT_EQ(p.id, id);
	for(size_t i = 0; i < p.values.size(); i++)
		EXPECT_EQ(p.values[i], values[i]);
}

TEST_F(PointTest, ConstructorChecksValuesNotEmpty)
{
	ASSERT_THROW(Point(123, {}), std::runtime_error);
}

TEST_F(PointTest, DistanceItself)
{
	auto p = Point(0, {1, 2, 3});
	EXPECT_EQ(p.distance(p), 0);
}

TEST_F(PointTest, DistanceOther)
{
	auto p1 = Point(0, {0, 0, 0});
	auto p2 = Point(0, {0, 0, 3.45});
	auto p3 = Point(0, {0, 1, 1});

	EXPECT_EQ(p1.distance(p2), 3.45);
	EXPECT_EQ(p1.distance(p3), std::sqrt(2));
}


class ClusterTest : public testing::Test
{};

TEST_F(ClusterTest, Constructor)
{
	auto values = std::vector<double>{1.1, -2.5, 3, 1000};
	auto c = Cluster(2, values);
	ASSERT_EQ(c.id, 2U);
	auto centroid = c.getCentroid();
	EXPECT_EQ(centroid.size(), values.size());
	for(size_t i = 0; i < centroid.size(); i++)
		EXPECT_EQ(values[i], centroid[i]);
}

TEST_F(ClusterTest, ConstructorChecksValuesNotEmpty)
{
	ASSERT_THROW(Cluster(123, {}), std::runtime_error);
}

TEST_F(ClusterTest, Add)
{
	auto values = std::vector<double>{1, 1};
	auto c = Cluster(0, values);
	auto p1 = std::make_shared<Point>(0, std::vector<double>{1, 1});

	EXPECT_EQ(c.getPoints().size(), 0U);
	c.addPoint(p1);
	EXPECT_EQ(c.getPoints().size(), 1U); // The cluster should have a point now
}

TEST_F(ClusterTest, SetCentroid)
{
	auto values = std::vector<double>{1, 1};
	auto c = Cluster(0, values);
	auto p1 = std::make_shared<Point>(0, std::vector<double>{1, 1, 1});

	ASSERT_THROW(c.set_centroid({}), std::runtime_error);
	c.set_centroid({1,2,3});
	ASSERT_EQ(c.getCentroid().size(), 3U);
	c.addPoint(p1);
	ASSERT_THROW(c.set_centroid(values), std::runtime_error);
}

TEST_F(ClusterTest, Remove)
{
	auto values = std::vector<double>{1, 1};
	auto c = Cluster(0, values);
	auto p1 = std::make_shared<Point>(0, std::vector<double>{1, 1});

	c.addPoint(p1);
	c.removePoint(p1);
	EXPECT_EQ(c.getPoints().size(), 0U);
}

TEST_F(ClusterTest, Update)
{
	auto values = std::vector<double>{1, 1};
	auto c = Cluster(0, values);
	const auto &centroid = c.getCentroid();
	auto p1 = Point::create(0, {1, 1});
	auto p2 = Point::create(1, {2, 2});

	c.addPoint(p1);
	c.updateMeans();
	for(size_t i = 0; i < centroid.size(); i++)
		EXPECT_EQ(p1->values[i], centroid[i]);

	c.addPoint(p2);
	c.updateMeans();
	for(size_t i = 0; i < centroid.size(); i++)
		EXPECT_EQ((p1->values[i] + p2->values[i]) / 2, centroid[i]);

	c.removePoint(p2);
	c.updateMeans();
	for(size_t i = 0; i < centroid.size(); i++)
		EXPECT_EQ(p1->values[i], centroid[i]);
}

TEST_F(ClusterTest, AddDoesNotUpdate)
{
	auto values = std::vector<double>{1, 1};
	auto c = Cluster(0, values);
	const auto &centroid = c.getCentroid();
	auto p1 = Point::create(0, {1, 1});

	c.addPoint(p1);
	for(size_t i = 0; i < centroid.size(); i++)
		EXPECT_EQ(values[i], centroid[i]);
}

TEST_F(ClusterTest, RemoveDoesNotUpdate)
{
	auto values = std::vector<double>{1, 1};
	auto c = Cluster(0, values);
	const auto &centroid = c.getCentroid();
	auto p1 = Point::create(0, {1, 1});

	c.addPoint(p1);
	c.updateMeans();
	c.removePoint(p1);
	for(size_t i = 0; i < centroid.size(); i++)
		EXPECT_EQ(p1->values[i], centroid[i]);
}

TEST_F(ClusterTest, UpdateOnEmpty)
{
	auto values = std::vector<double>{1, 1};
	auto c = Cluster(0, values);
	const auto &centroid = c.getCentroid();

	c.updateMeans();

	// Centroid should be NAN
	for(size_t i = 0; i < centroid.size(); i++)
		EXPECT_TRUE(std::isnan(centroid[i]));
}

TEST_F(ClusterTest, RemoveUnexistent)
{
	auto c = Cluster(0, {1, 2});
	ASSERT_THROW(c.removePoint(NULL), std::runtime_error);
	ASSERT_THROW(c.removePoint(Point::create(1, {2, 3})), std::runtime_error);
}

TEST_F(ClusterTest, DifferentDimensions)
{
	auto c = Cluster(0, {1, 2});
	auto p1 = Point::create(1, {1, 2, 3});
	auto p2 = Point::create(2, {1});
	ASSERT_THROW(c.addPoint(p1), std::runtime_error);
	ASSERT_THROW(c.addPoint(p2), std::runtime_error);
}

TEST_F(ClusterTest, AddNullPoint)
{
	auto c = Cluster(0, {1, 2});
	ASSERT_THROW(c.addPoint(NULL), std::runtime_error);
}


class ClusterPairwiseDistanceTest : public testing::Test
{
	protected:
	Cluster c = Cluster(0, {0, 0, 0});
	std::vector<point_ptr_t> points =
	{
		Point::create(-1U, {0,    0, 2}),
		Point::create(-2U, {0,    2, 0}),
		Point::create(-3U, {1,  0.2, 0}),
		Point::create(-4U, {1,  0.2, 0}),
		Point::create(-5U, {1, -0.6, 0}),
		Point::create(-6U, {1, -1.4, 1}),
		Point::create(-7U, {1,  1.8, 4}),
		Point::create(-8U, {1,  3.5, 4}),
	};

	virtual void SetUp()
	{
		for (const point_ptr_t point : points)
			c.addPoint(point);
		c.updateMeans();
	}
};

TEST_F(ClusterPairwiseDistanceTest, EmptyCluster)
{
	auto empty = Cluster(0, {0, 0, 0});
	auto p = Point::create(0, {1, 2, 3});
	ASSERT_THROW(empty.pairwise_distance(p), std::runtime_error);
	ASSERT_THROW(empty.pairwise_distance(), std::runtime_error);
}

// Pairwise distance of a point to all the points in the cluster.
// The first point is not in the cluster.
TEST_F(ClusterPairwiseDistanceTest, Point)
{
	const auto p = Point::create(0, {0, 0, 0});
	const auto distances = c.pairwise_distance(p);
	const auto &points = c.getPoints();
	for (const auto &item : distances)
	{
		const point_ptr_t p1 = item.first.first;
		const point_ptr_t p2 = item.first.second;
		double dist = item.second;
		ASSERT_EQ(p, p1);
		ASSERT_TRUE(points.count(p1) == 0); // p1 is not in the cluster
		ASSERT_TRUE(points.count(p2) != 0);
		ASSERT_EQ(dist, p1->distance(*p2));
	}
	ASSERT_EQ(distances.size(), points.size());
}

// Pairwise distance of all the points in a cluster
TEST_F(ClusterPairwiseDistanceTest, NoPoint)
{
	auto distances = c.pairwise_distance();
	const auto &points = c.getPoints();
	for (const auto &item : distances)
	{
		const point_ptr_t p1 = item.first.first;
		const point_ptr_t p2 = item.first.second;
		double dist = item.second;
		ASSERT_TRUE(points.count(p1) != 0);
		ASSERT_TRUE(points.count(p2) != 0);
		ASSERT_EQ(dist, p1->distance(*p2));
	}
}

TEST_F(ClusterPairwiseDistanceTest, PointIsInCluster)
{
	auto p = Point::create(0, {0, 0, 0});
	auto distances = c.pairwise_distance(p);
	const auto &points = c.getPoints();
	c.addPoint(p);
	for (const auto &item : distances)
	{
		const point_ptr_t p1 = item.first.first;
		const point_ptr_t p2 = item.first.second;
		double dist = item.second;
		ASSERT_EQ(p, p1);
		ASSERT_TRUE(points.count(p1) != 0);
		ASSERT_TRUE(points.count(p2) != 0);
		ASSERT_EQ(dist, p1->distance(*p2));
	}
}

TEST_F(ClusterPairwiseDistanceTest, Mean)
{
	auto c1 = Cluster(1, {0, 0, 0});
	std::vector<point_ptr_t> aux =
	{
			Point::create(-1U, {0, 0, 1}),
			Point::create(-2U, {0, 1, 0}),
			Point::create(-3U, {1, 0, 0}),
	};

	for (const auto &point : aux)
		c1.addPoint(point);

	double dist = c1.mean_pairwise_distance();

	ASSERT_EQ(dist, std::sqrt(2));
}

TEST_F(ClusterPairwiseDistanceTest, Min)
{
	auto c1 = Cluster(1, {0, 0, 0});
	std::vector<point_ptr_t> aux =
	{
			Point::create(-1U, {0, 0, 1}),
			Point::create(-2U, {0, 1, 0}),
			Point::create(-3U, {10, 0, 0}),
	};

	for (const auto &point : aux)
		c1.addPoint(point);

	const double dist = c1.min_pairwise_distance();

	ASSERT_EQ(dist, std::sqrt(2));
}

TEST_F(ClusterPairwiseDistanceTest, Max)
{
	auto c1 = Cluster(1, {0, 0, 0});
	std::vector<point_ptr_t> aux =
	{
			Point::create(-1U, {0, 0, 1}),
			Point::create(-2U, {0, 1, 0}),
			Point::create(-3U, {10, 0, 0}),
	};

	for (const auto &point : aux)
		c1.addPoint(point);

	const double dist = c1.max_pairwise_distance();

	ASSERT_EQ(dist, std::sqrt(101));
}

TEST_F(ClusterPairwiseDistanceTest, MeanPoint)
{
	auto c1 = Cluster(1, {0, 0, 0});
	auto p = Point::create(0, {0, 0, 0});
	std::vector<point_ptr_t> aux =
	{
			Point::create(-1U, {0, 0, 1}),
			Point::create(-2U, {0, 1, 0}),
			Point::create(-3U, {1, 0, 0}),
	};

	for (const auto &point : aux)
		c1.addPoint(point);

	const double dist = c1.mean_pairwise_distance(p);

	ASSERT_EQ(dist, 1);
}

TEST_F(ClusterPairwiseDistanceTest, MinPoint)
{
	auto c1 = Cluster(1, {0, 0, 0});
	auto p = Point::create(0, {0, 0, 0});
	std::vector<point_ptr_t> aux =
	{
			Point::create(-1U, {0, 0, -0.1}),
			Point::create(-2U, {0, 1, 0}),
			Point::create(-3U, {10, 0, 0}),
	};

	for (const auto &point : aux)
		c1.addPoint(point);

	const double dist = c1.min_pairwise_distance(p);

	ASSERT_EQ(dist, 0.1);
}

TEST_F(ClusterPairwiseDistanceTest, MaxPoint)
{
	auto c1 = Cluster(1, {0, 0, 0});
	auto p = Point::create(0, {0, 0, 0});
	std::vector<point_ptr_t> aux =
	{
			Point::create(-1U, {0, 0, 1}),
			Point::create(-2U, {0, 1, 0}),
			Point::create(-3U, {-10, 0, 0}),
	};

	for (const auto &point : aux)
		c1.addPoint(point);

	const double dist = c1.max_pairwise_distance(p);

	ASSERT_EQ(dist, 10);
}


TEST(InterclusterDistanceTest, CentroidToCentroid)
{
	auto c1 = Cluster(1, {1, 2, 1});
	auto c2 = Cluster(2, {0.3, 0.1, -0.5});
	auto p1 = c1.getCentroid();
	auto p2 = c2.getCentroid();
	double dist = 0;
	for (size_t i = 0; i < p1.size(); i++)
		dist += pow(p1[i] - p2[i], 2);
	ASSERT_EQ(c1.centroid_to_centroid_distance(c2), std::sqrt(dist));
}

TEST(InterclusterDistanceTest, CheckDisjoint)
{
	auto c1 = Cluster(1, {0, 0, 0});
	auto c2 = Cluster(0, {0, 0, 0});
	auto p = Point::create(0, {0, 0, 0});
	std::vector<point_ptr_t> aux1 =
	{
			Point::create(1U, {0, 0, 1}),
			Point::create(2U, {0, 0, 2}),
			Point::create(3U, {0, 0, 3}),
	};
	std::vector<point_ptr_t> aux2 =
	{
			Point::create(1U, {1, 0, 1}),
			Point::create(2U, {0, 100, 0}),
			Point::create(3U, {110, 0, 0}),
	};

	ASSERT_TRUE(c1.disjoint(c2));
	c1.addPoint(p);
	c2.addPoint(p);
	ASSERT_FALSE(c1.disjoint(c2));
}

TEST(InterclusterDistanceTest, ClosestPoints)
{
	auto c1 = Cluster(1, {0, 0, 0});
	auto c2 = Cluster(0, {0, 0, 0});
	auto p = Point::create(0, {1, 11, 111});
	std::vector<point_ptr_t> aux1 =
	{
			Point::create(1U, {0, 0, 1}),
			Point::create(2U, {0, 0, 2}),
			Point::create(3U, {0, 0, 3}),
	};
	std::vector<point_ptr_t> aux2 =
	{
			Point::create(1U, {1, 0, 1}),
			Point::create(2U, {0, 100, 0}),
			Point::create(3U, {110, 0, 0}),
	};
	for (const point_ptr_t point : aux1)
		c1.addPoint(point);
	for (const point_ptr_t point : aux2)
		c2.addPoint(point);
	ASSERT_EQ(c1.closest_points_distance(c2), 1);

	// Now the clusters share a point so the distance should be 0
	c1.addPoint(p);
	c2.addPoint(p);
	ASSERT_EQ(c1.closest_points_distance(c2), 0);
}

TEST(InterclusterDistanceTest, FarthestPoints)
{
	auto c1 = Cluster(1, {0, 0, 0});
	auto c2 = Cluster(0, {0, 0, 0});
	auto p = Point::create(0, {1, 1, 1});
	std::vector<point_ptr_t> aux1 =
	{
			Point::create(-1U, {1, 2, 6}),
			Point::create(-2U, {2, 2, 6}),
			Point::create(-3U, {10, 0, 0}),
	};
	std::vector<point_ptr_t> aux2 =
	{
			Point::create(-1U, {0, 50, -0.1}),
			Point::create(-2U, {0, 100, -0.1}),
			Point::create(-3U, {11, 0, 0}),
	};
	for (const auto &point : aux1)
		c1.addPoint(point);
	for (const auto &point : aux2)
		c2.addPoint(point);
	ASSERT_EQ(c1.farthest_points_distance(c2), std::sqrt(100 + 10000 + 0.01));

	// Now the clusters share a point
	c1.addPoint(p);
	c2.addPoint(p);
	ASSERT_EQ(c1.farthest_points_distance(c2), std::sqrt(100 + 10000 + 0.01));
}


class KMeansTest : public testing::Test, public KMeans
{
	protected:

	std::vector<Cluster> clusters =
	{
			Cluster(0, {10, 10, 10}),
			Cluster(1, {20, 20, 20}),
			Cluster(2, {30, 30, 30}),
	};
};

TEST_F(KMeansTest, NearestCluster)
{
	Point p1 = {0, {11,11,11}};
	Point p2 = {1, {21,21,21}};
	Point p3 = {2, {31,31,31}};

	ASSERT_EQ(KMeans::nearestCluster(clusters, p1), 0);
	ASSERT_EQ(KMeans::nearestCluster(clusters, p2), 1);
	ASSERT_EQ(KMeans::nearestCluster(clusters, p3), 2);
}

TEST_F(KMeansTest, InitClusters)
{
	auto p1 = Point::create(0, {11,11,11});
	auto p2 = Point::create(1, {21,21,21});
	auto p3 = Point::create(2, {31,31,31});
	std::vector<point_ptr_t> points = {p1, p2, p3} ;

	KMeans::initClusters(points.size(), points, clusters);

	ASSERT_EQ(clusters.size(), points.size());
	for (size_t i = 0; i < clusters.size(); i++)
	{
		ASSERT_EQ(clusters[i].centroid_distance(*points[i]), 0);
		ASSERT_EQ(clusters[i].getPoints().size(), 0U);
	}
}

TEST_F(KMeansTest, Clusterize)
{
	std::vector<Cluster> clusters =
	{
			Cluster(0, {0}),
			Cluster(1, {0}),
			Cluster(2, {0}),
			Cluster(3, {0})
	};
	std::vector<point_ptr_t> points =
	{
		Point::create(1, {369342069}),
		Point::create(2, {236950595}),
		Point::create(3, {374689971}),
		Point::create(4, {23961389}),
		Point::create(5, {5440233}),
		Point::create(6, {241292687}),
		Point::create(7, {25111863}),
		Point::create(8, {194790544})
	};
	size_t iters = KMeans::clusterize(4, points, clusters, 5);

	size_t count = 0;
	for (const auto &c : clusters)
	{
		ASSERT_TRUE(c.getPoints().size() > 0);
		count += c.getPoints().size();
	}
	ASSERT_EQ(points.size(), count);

	ASSERT_TRUE(iters < 5U);
}


	// double silhouette(const std::vector<Cluster> &clusters);
	// size_t clusterize(size_t k, const std::vector<Point> &points, std::vector<Cluster> &clusters, size_t max_iter);
	// size_t clusterize_optimally(size_t max_k, const std::vector<Point> &points, std::vector<Cluster> &clusters, size_t max_iter);
