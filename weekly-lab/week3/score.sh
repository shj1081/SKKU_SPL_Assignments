echo "$1+$2" | bc > answer.txt
echo "$1-$2" | bc >> answer.txt
make w3
echo "$1 $2 " | ./w3 > output.txt
diff answer.txt output.txt > result.txt
make clean