 #!/bin/bash
echo
make clean
echo 
make
echo
echo Sending Files ......
scp -r $PWD/*.ko $PWD/main $PWD/faltu_main $PWD/*.h root@192.168.1.5:/home/root
echo 
#scp -r $PWD/(main) root@192.168.1.5:/home/root