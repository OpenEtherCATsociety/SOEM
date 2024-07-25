# SOEM(Simple Open EtherCAT Master Library)を利用したGOモータとの通信プログラム

## GOモータ単体テストとしての動作方法（このリポジトリ単体で動作可能）

ビルド
```
$ cd /path_to_soem
$ mkdir build
$ cd build
$ cmake ..
$ make
```

実行
```
$ cd /path_to_soem
$ ./cat_test.sh
```
（もし`No socket connection on enp45s0`というエラーメッセージとともに失敗するようであれば、`cat_test.sh`の中に書いてあるethernet interfaceを変更して試すこと。各PCに対して有効なethernet interfaceは、`$ip a`コマンドで確認できる）

## ロボット取付GOモータへの通信プログラムとしての動作方法（ロボット用プログラムリポジトリのsubmoduleとしてのみ利用可能）

ビルド
```
$ cd /path_to_soem
$ mkdir build
$ cd build
$ cmake .. -DBUILD_ROBOT_PROGRAM=ON
$ make
```
実行
```
$ sudo setcap cap_net_raw=eip ./build/test/linux/proxima_robot/proxima_robot
$ ./build/test/linux/proxima_robot/proxima_robot enp45s0
```
