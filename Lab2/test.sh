echo "Точки,Потоки,Время" > results.csv
for points in 1000 5000 10000; do 
	for threads in 1 2 4 8; do
		echo "Тестирование: $points точек, $threads потоков"
		echo "5" > test_data.txt
		echo "$points" >> test_data.txt
		for ((i=0; i<points; i++)); do
			echo "$((RANDOM % 100)).$((RANDOM % 100)) $((RANDOM % 100)).$((RANDOM % 100))" >> test_data.txt
		done

		time=$(./a.out -t $threads < test_data.txt 2>&1 | grep "Время выполнения" | awk '{print $3}')
		echo "$points,$threads,$time" >> results.csv
	done
done
