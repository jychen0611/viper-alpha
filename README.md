# viper: A Virtual Ether-Switch for Linux.

<img src="viper.png" width="20%"/>

## vscode c_cpp_properties

ctrl+shift+p --> c/c++ edit configuration --> c_cpp_properties.json

```shell
$ ls /usr/src
linux-headers-6.8.0-40-generic  linux-hwe-6.8-headers-6.8.0-40
linux-headers-6.8.0-49-generic  linux-hwe-6.8-headers-6.8.0-49
$ uname -r
6.8.0-49-generic
$ gcc --version
gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
Copyright (C) 2021 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```
`c_cpp_properties.json`
```json
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "/usr/include",
                "/usr/local/include",
                "/usr/src/linux-headers-6.8.0-49-generic/include",
                "/usr/src/linux-headers-6.8.0-49-generic/arch/x86/include",
                "/usr/src/linux-headers-6.8.0-49-generic/arch/x86/include/generated",
                "/usr/lib/gcc"
            ],
            "defines": [
                "__GNUC__",
                "__KERNEL__",
                "MODULE"
            ],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "c99",
            "cppStandard": "c++98",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
```

## clang-format

```shell
$ sudo apt install clang-format
```
automatically format C and header files in the current directory
```shell
$ clang-format -i *.[ch]
```

## test
```shell
$ chmod +x ./tools/test.sh
$ sudo ./tools/test.sh
```

```shell
sudo dmesg
```