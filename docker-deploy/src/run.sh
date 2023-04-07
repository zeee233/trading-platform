make clean
make all
echo 'start running server...'
chmod o+x test
chmod o+x client
taskset --cpu-list 0-1 ./test
while true ; do continue ; done