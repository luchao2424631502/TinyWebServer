# TinyHTTPServer源码分析

## 目录树:

源码目录:

1. 日志功能 log/
2. 数据库连接池sql/
3. 线程池 include/threadpool.h

 	4. 定时器 timer/:
 	5. 封装的基本的线程同步机制:locker/ 
 	6. http协议解析: http/ 
 	7. 运行选项配置 config/

8. 头文件 include/
9. 资源文件 root/

提供8个配置选项:

> * -p 指定监听端口
> * -l 0同步写入日志, 1异步写入日志
> * -o 1:开启socket linger延迟关闭连接 0:关闭
> * -s 8 数据库连接池数量
> * -t 线程池内线程的数量
> * -c 0:开启日志 1:关闭日志
> * -a IO模型 1:reactor 0:proactor

---

### 构建

项目依赖: 

1. Mysql C api apt:`sudo apt install libmysqlclient` 
2. pthread

修改项目`main.cc`中的登陆mysql的用户名和密码,以及库名

表结构

```sql
SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for user
-- ----------------------------
DROP TABLE IF EXISTS `user`;
CREATE TABLE `user` (
  `username` char(50) DEFAULT NULL,
  `passwd` char(50) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

SET FOREIGN_KEY_CHECKS = 1;
```

编译:(重写了makefile,0 warning)下载后进入项目根目录执行`make`

执行: `./server -p 8080`

清除:`make clean`

> 如果有错误注意当前文件夹的权限问题,gdb看看可能是创建日志文件权限不够

-----

### 大概工作流程:

Config类解析配置,个个模块类最后都整合进WebServer类,方便进程运行时调用

server整体的工作流程: config解析命令行选项->启动日志->初始化sql连接池->初始化线程池->监听->处理http请求->返回



## 模块功能解析

### /locker

将信号量`sem_t`封装成类`sem`,互斥量`pthread_mutex_t`封装成`locker`,配合条件变量`pthread_cond_t` ->`cond`使用,并将`post wait lock unlock wait signal `等基本方法封装到类中

### /log

1. 同步:

    直接将产生的日志信息通过C的标准IO写入的日志文件

2. 异步:

   用数组封装一个先进先出的队列,然后将产生的每一条日志信息写入到队列,开启一个线程来不断读取队列中的每一条信息

### /config 

​	使用`getopt()`函数来解析命令行选项

### include/threadpool.h

​	用到了locker(线程同步)和sql(某页面使用了注册和登陆功能需要查询数据库),

​	线程池由 请求队列+多个线程 组成

> 主线程接受到读写事件发生的通知后:
>
> reactor模式: 将所有的工作都交给工作的线程来处理: IO + http解析(业务逻辑)
>
> ​						过程: 调整定时器在lst的位置和加入到线程池的任务队列
>
> proactor模式: 在主线程中处理IO(read/write):然后将工作(http解析,业务)交给线程的处理
>
> ​						过程:主线程在处理完read(注意write不需要加入到线程池的任务队列中,因为在read完成后加入到任务队列已经完成了http的解析,所以返回给客户端就行),交给线程来处理http解析,然后修改监听事件类型,写返回给客户端

​	多个线程会根据信号量的值来竞争处理任务队列的任务(http class也封装了IO操作)

​	worker函数声明为`static`就不会有隐含的this指针作为参数,和void*参数不匹配

### /sql

​	init()中调用mysql_real_connect()获取**MYSQL***连接,生成多个可用的sql连接

### /timer

​	client_data存client的IP+port以及util_timer的指针,util_timer也指向对应连接的client_data,所有的连接维护在sort_timer_lst链表中,(根据expire超时时间)	

​	通过sockpair和信号处理函数将信号转化为IO事件,

​	cb_func回调函数用来清除超时的连接

### /http

​	修改了原项目的http_conn::write(),(原来的发送好像有点问题[]())

​	解析流程看看[有限状态自动机]([http://luchao.wiki/2020/10/30/csdn/%E6%9C%89%E9%99%90%E7%8A%B6%E6%80%81%E6%9C%BA%E5%AE%9E%E4%BE%8B!%E6%9C%8D%E5%8A%A1%E7%AB%AF%E5%AE%9E%E7%8E%B0%E7%AE%80%E5%8D%95%E7%9A%84HTTP%E8%AF%B7%E6%B1%82%E7%9A%84%E8%AF%BB%E5%8F%96%E5%92%8C%E5%88%86%E6%9E%90/](http://luchao.wiki/2020/10/30/csdn/有限状态机实例!服务端实现简单的HTTP请求的读取和分析/)),

​	![1](https://mmbiz.qpic.cn/mmbiz_jpg/6OkibcrXVmBH2ZO50WrURwTiaNKTH7tCia3AR4WeKu2EEzSgKibXzG4oa4WaPfGutwBqCJtemia3rc5V1wupvOLFjzQ/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

​	![2](https://mmbiz.qpic.cn/mmbiz_jpg/6OkibcrXVmBG9ibQZ4SgllXZqrkObpUHNKNoh8SsGMyOSGIgaE8nZdGhYua3E84VojicmKuJoict9s3ibraK6Lux1dQ/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

​	http包的组成:请求行+请求头部字段+空行+body

​	整个http的解析流程封装在函数process()中;

------

## webbench测试数据

* 只放在本地的虚拟机里面测试了,不准确

  CPU: AMD Ryzen 5 2500U with Radeon Vega Mobile 
  Memory: 6917MiB 

```c
webbench -c 8 -t 60:
1.LT + LT:-m 0 -t 8 -s 8:
                176432 pages/min 329339 bytes/sec 
                188502 pages/min 351870 bytes/sec
                189271 pages/min 353307 bytes/sec
                190430 pages/min 355469 bytes/sec
                191499 pages/min 357464 bytes/sec
                191771 pages/min 357974 bytes/sec
                189068 pages/min 352926 bytes/sec

2.LT + ET:-m 1 -t 8 -s 8:
                180609 pages/min 337136 bytes/sec               
                188382 pages/min 351646 bytes/sec
                202035 pages/min 377133 bytes/sec
                202198 pages/min 377436 bytes/sec
                193193 pages/min 360460 bytes/sec
                
3.ET + LT:-m 2 -t 8 -s 8:
                217595 pages/min 406177 bytes/sec
                214466 pages/min 400336 bytes/sec

4.ET + ET:-m 3 -t 8 -s 8:
                213698 pages/min 398904 bytes/sec
                207289 pages/min 386939 bytes/sec

--------------------------------------------------
webbench -c 32 -t 10:
1.LT + LT:-m 0 -t 32 -s 32:
                181638 pages/min 339057 bytes/sec
                203268 pages/min 379444 bytes/sec
                197808 pages/min 369252 bytes/sec

2.LT + ET:-m 1 -t 32 -s 32:
                176514 pages/min 329481 bytes/sec
                190170 pages/min 354984 bytes/sec
                182838 pages/min 341308 bytes/sec

3.ET + LT:-m 2 -t 32 -s 32:
                197916 pages/min 369443 bytes/sec
                210204 pages/min 392392 bytes/sec
                212076 pages/min 395996 bytes/sec

4.ET + ET:-m 3 -t 32 -s 32:
                204384 pages/min 381516
                188784 pages/min 352396
                208128           388505
                
```



​	

​	

​	