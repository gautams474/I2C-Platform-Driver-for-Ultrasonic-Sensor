 #!/bin/sh
echo
make clean
echo 
make
echo
echo Sending Files- part 2......
scp -r $PWD/*.ko $PWD/main $PWD/script $PWD/*.h root@192.168.1.5:/home/root
echo 
#scp -r $PWD/(main) root@192.168.1.5:/home/root