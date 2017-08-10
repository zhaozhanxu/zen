#[ZEN](https://github.com/zhaozhanxu/zen) - A Datapath Platform Based DPDK

# 目录
---------------------------------------

* [简介](#简介)
	* [背景](#背景)
	* [功能](#功能)
	* [实现](#实现)
	* [特性](#特性)
	* [简单网络协议node顺序图](#简单网络协议node顺序图)
	* [功能清单](#功能清单)
* [运行前配置](#运行前配置)
	* [系统配置](#系统配置)
        * [驱动模块自动加载](#驱动模块自动加载)
        * [配置hugepage](#配置hugepage)
        * [自动绑定网卡服务](#自动绑定网卡服务)

# 简介
---------------------------------------

## 背景

因为工作需要接触到DPDK，但是DPDK只是一个高性能网络IO的框架，对网络协议没有支持，而我们在开发的过程中，没有一个称手的框架，所以一直想要自己写一个基于DPDK的高性能网络框架，今年有幸接触到Cisco的开源项目VPP，读来受益匪浅，但是因为整个项目比较复杂，所以自己想要自己写一个更适合自己使用，更加简单灵活的框架，适合后续的开发。

## 功能

该框架主要是基于DPDK的报文收发来做，所有的网络协议或者功能模块都是以plugin的模式来完成。而plugin中时功能逻辑实现的一些node，这些node通过配置文件创建关联关系。这样我们在开发的时候可以都以plugin的方式进行开发单元功能模块，而根据配置文件灵活配置单元模块的调用关系。
我能想到的可以用它来做网络协议栈、负载均衡、VRouter、GateWay、防DDOS之类软件。

## 实现

* 软件启动时首先加载DPDK的一些基本初始化
* 从指定的路径加载plugin，plugin中包含了一个功能node
* 解析node graph，该graph主要是列出了node之间的顺序关联
* 报文根据node中逻辑判断next node，然后将报文传递给next node，并且将next node加入到runtime node list。
* 顺序执行runtime node list中的node，处理报文，知道list完成。

## 特性

我期望该平台能做以下事情:

* 网络的各个协议模块尽量解耦
* 添加协议模块能够相对简单一些
* 框架尽量做到不修改
* 配置灵活
* 模块尽量可以复用

## 简单网络协议node顺序图

```xml
[ethernet-input]
0 = ethernet_output
1 = arp-input
2 = ip4-input
[ethernet-output]
[arp-input]
[ip4-input]
0 = ip4-local
1 = ip4-lookup
[ip4-local]
0 = ip4-icmp-input
1 = ip4-udp-input
2 = ip4-tcp-input
[ip4-lookup]
0 = ip4-output
[ip4-output]
0 = ethernet-output
```

## 功能清单

- [ ] 基本框架


# 运行前配置
---------------------------------------

## 系统配置

### 驱动模块自动加载

创建文件`/etc/modules-load.d/igb_uio.conf`，内容如下：

```xml
# Load uio.ko at boot
uio
# Load igb_uio.ko at boot
igb_uio
```

### 配置hugepage

如果系统支持1G的hugepage，并且想要使用的话，需要在`/etc/default/grub`修改内容如下：

```xml
# 该配置最后添加default_hugepagesz=1G
GRUB_CMDLINE_LINUX="xxxxxxxxxxxxxxxxxxxxxx default_hugepagesz=1G"
```
执行命令`grub2-mkconfig -o /boot/grub2/grub.cfg`。

配置hugepage，需要在`/etc/sysctl.conf`添加如下配置：

```xml
# 此处可配可不配，默认是65535
# 如果下面hugepage小于32767，不会出问题
vm.max_map_count=2048000
# 此处必须配置，根据需求配置大小
vm.nr_hugepages = 102400
```

最后执行`sysctl -p`。

### 自动绑定网卡服务

这个已经在代码里面做了，所以这里的配置完全可以忽略，我个人记录一下以前的做法。

创建文件到`/etc/init.d/bind_nics`，内容如下：

```xml
#! /bin/bash
# add for chkconfig
# chkconfig: 2345 70 30
# description: bind nics
# processname: bind_nics

# 此处的driver和网卡信息都需要根据自己系统进行修修改
DRIVER=ixgbe
NIC1=0000:02:05.0
NIC2=0000:02:06.0

start() {
	if [ ! -f $NIC1 ]; then
		/sbin/dpdk-devbind -b igb_uio $NIC1
	fi
	if [ ! -f $NIC2 ]; then
		/sbin/dpdk-devbind -b igb_uio $NIC2
	fi
}

stop() {
	if [ ! -f $NIC1 ]; then
		/sbin/dpdk-devbind -u $NIC1
		/sbin/dpdk-devbind -b $DRIVER $NIC1
	fi
	if [ ! -f $NIC2 ]; then
		/sbin/dpdk-devbind -u $NIC2
		/sbin/dpdk-devbind -b $DRIVER $NIC2
	fi
}

case "$1" in
	start)
		start
		;;

	stop)
		stop
		;;

	restart)
		stop
		start
		;;

	*)
		echo
		$"Usage: $0 {start|stop|restart}"
		exit 1
esac
```

执行如下命令：

```bash
chmod 755 /etc/init.d/bind_nics
chkconfig --add bind_nics
systemctl start bind_nics
```

