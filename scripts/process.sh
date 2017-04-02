for i in slowfirst slowfirst_kmeans slowfirst_kmeans2 slowfirst_kmeans_model_3k slowfirst_kmeans_model_4k silhouette slowfirst_kmeans_model_4k_bothsides; do
	cd $i/data
	echo $i
	sudo chown -R viselol:sudo .
	python3 ../../../scripts/process.py ../../workloads.yaml $i
	cd -
done
