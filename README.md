
Ringbuffer
==========

用于缓冲I/O流数据的[循环队列](http://en.wikipedia.org/wiki/Circular_buffer)，参照了linux [kfifo](http://lxr.free-electrons.com/source/include/linux/kfifo.h)；

这里的实现不是所有连接共享同一Ringbuffer，如[这篇文章](http://blog.codingnow.com/2012/02/ring_buffer.html)及其[实现](http://blog.codingnow.com/2012/04/mread.html)，这里的实现类似于Muduo [Buffer](http://blog.csdn.net/solstice/article/details/6329080)的设计，每个连接两个buffer，一个负责读一个负责写。

rb_t
----

`rb_t`就是一次性分配的一段地址空间连续的内存；

生产过程:

* 首先从对应的buffer里获取(`rb_producer_peek`)一段空闲缓存，然后填充这段缓存，最后将这段缓存设置为己使用(`rb_produced`)；

消费过程:

* 首先从对应的buffer里获取(`rb_consumer_peek`)一段己使用缓存，然后处理这段缓存，最后将这段缓存设置为空闲(`rb_produced`)；

在网络事件中:

* 当读事件产生时，执行生产过程，操作的是该连接的读buffer，调用相关的`read`接口来填充缓存；
* 当应用程序执行读操作时，执行消费过程，操作的是该连接的读buffer；
* 当写事件产生时，执行消费过程，操作的是该连接的写buffer，调用相关的`write`接口来发送缓存；
* 当应用程序执行写操作时，执行生产过程，操作的是该连接的写buffer；

这样在实现IO缓存的同时也实现了buffer到应用程序、应用程序到buffer的零拷贝；

如果生产或消费的数据长度已经确定，那么可以获取(`rb_producer_peek` / `rb_consumer_peek`)并eaten(`rb_produced` / `rb_consumed`)这段缓存，再进行IO操作；

读事件产生时直接调用`rb_read`，设置读回调函数`read_cb`:

```c
typedef int(*rb_read_pt)(void *, void *, unsigned int);
int rb_read(rb_t *rb, rb_read_pt read_cb, void *ptr, unsigned int *read);
```

`rb_read`将循环调用`read_cb`直至缓存不足或者回调返回错误，例:

```c
static int recvfd(void *ptr, void *buf, unsigned int size)
{
    int fd, rv;

    fd = *(int *)ptr;
    do {
        rv = recv(fd, buf, size, 0);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return -EAGAIN;
        else
            return -errno;
    }

    return rv;
}
```

写事件产生时直接调用`rb_write`，设置写回调函数`write_cb`:

```c
typedef int(*rb_write_pt)(void *, const void *, unsigned int);
int rb_write(rb_t *rb, rb_write_pt write_cb, void *ptr, unsigned int *wrote);
```

`rb_write`将循环调用`write_cb`直至缓存清空或者回调返回错误，例:

```c
static int sendfd(void *ptr, const void *buf, unsigned int size)
{
    int fd, rv;

    fd = *(int *)ptr;
    do {
        rv = send(fd, buf, size, 0);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return -EAGAIN;
        else
            return -errno;
    }

    return rv;
}
```

rbvec_t
-------

`rbvec_t`是`rb_t`的进一步扩展，解决内存扩展及缓存不连续的问题，`rb_t`已经具备了主要功能，当然可以不使用`rbvec_t`；

`rb_t`的两个问题:

* 没有扩展缓存的操作，如果缓存己满将无法写入；
* 如果Ringbuffer存在回绕，那么可能需要执行两次生产或者消费过程才能生产掉或者消费掉全部数据；

  例如:

       当读事件产生时，如果连接上的可读数据长度大于获取的缓存长度，那么将无法一次性recv全部数据，需要再次执行生产过程才能读入全部数据，这样增加了系统调用次数，当然在水平触发模式下也可以不执行并返回等待下次读事件，但效率更差。

`rbvec_t`是管理一个由`rb_t`组成的链表，解决了`rb_t`无法扩展缓存的问题，当生产时缓存不足并且长度未达到最大限制，则扩展当前缓存长度的新缓存（扩展后长度=扩展前长度*2）；

生产与消费过程与`rb_t`大体相同，这里获取不再是一段地址空间连续的缓存而是[`struct iovec`](http://www.gnu.org/software/libc/manual/html_node/Scatter_002dGather.html)，IO操作使用`readv`/`writev`来替代，这样减少了系统调用次数并且zerocopy，具体见wiki [scatter/gather I/O](http://en.wikipedia.org/wiki/Vectored_I/O)、以及[Fast Scatter-Gather I/O](http://www.gnu.org/software/libc/manual/html_node/Scatter_002dGather.html)、另外Muduo [Buffer](http://blog.csdn.net/solstice/article/details/6329080)也使用了这种方案。

线程安全
--------

非线程安全，这里考虑的网络模型，buffer应该只存在于线程内，多线程不会共享一个buffer，所以没有实现线程安全；

`rb_t`可以一个生产线程和一个消费线程并发。

回绕
----

就是生产或消费移动至循环队列尾边界时，为防止溢出回绕到头部继续。

----

Ideas TODO..
-----------
* Ringbuffer + [http-parser](https://github.com/nicolasff/webdis) + some event loop ..

