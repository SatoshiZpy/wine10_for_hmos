# 代码拉取
```
git clone https://gitcode.com/ohos-wine/wine.git -b ohos-wine-10.12
cd wine
git submodule update --init
git lfs pull
```
# 环境配置
新开发环境上需要安装以下开发工具。
```
sudo apt update 
sudo apt install unzip
sudo apt install make
sudo apt install autoconf
sudo apt install build-essential
sudo apt install flex
sudo apt install bison
sudo apt install llvm
sudo apt install git-lfs

sudo dpkg --add-architecture i386
sudo apt update
sudo apt install gcc-multilib g++-multilib
sudo apt install -y libfreetype6-dev:i386
```
# 编译构建
产物在out目录下，同时拷贝到`ohos\app\wine_engine\libs`和`ohos\app\wine_engine\src\main\resources\resfile`路径下。
```
./build.sh
```
# 运行
1. 在[华为开发者联盟](https://developer.huawei.com/consumer/cn/download/)下载Deveco Studio。
2. 使用Deveco Studio打开`ohos\app`。
3. 将编译产物`ohos\app\wine_engine\libs`和`ohos\app\wine_engine\src\main\resources\resfile`替换到`ohos\app`内对应路径。
4. 运行。