 Demux -> Decode -> Encode -> Muxer

## Install libopus

```sh
sudo apt-get install libopus-dev
```


## Clone FFmpeg and build

```sh
git clone https://github.com/FFmpeg/FFmpeg.git
cd FFmpeg
./configure --enable-libopus
make
sudo make install
```


## Build

```sh
git clone https://github.com/WadeLiu/videosoft_work.git
cd videosoft_work
mkdir build && cd build
cmake ..
make
./transcoding -i input_file -o output_file
```
