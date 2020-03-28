student ID: 517021911099

name:  赵旭阳

e-mail address: brokenbones@sjtu.edu.cn

### lab1 reliable data transporation.

- 选择的protocol: Go back N
- checksum算法：crc16 algorithm from https://github.com/lammertb/libcrc/blob/master/src/crc16.c

**包的格式设计**

|<- 2 byte        ->|<- 1 byte     ->|<- 1 byte     ->|<-      RDTSIZE-4    ->|

|<- checksum ->| payload size |<- seqnum ->|<-       payload       ->|

最前面是2byte的校验码，然后是1byte的包大小和1byte的序列号，最后跟着RDT_PKTSIZE - 4 byte的payload

序列号的范围是0～127, 序列号的最高位表示该包是否是一个messgae的最后一个包

ack包的payload为空

**Sender逻辑**

- 收到Network层新Message

  将message拆解并打包成多个packet，按序发出，使用nbuffered(表示正在传输的包数), next_ack(表示下一个等待确认送达的包)来维护sliding window，如果当前sliding window已满，就将待发送的包存在内存的waiting buffer中。

  对每一个发出的包，开启计时，设计超时时长为0.3ms。对于计时器，使用Timer chain的方法实现，仅下一个计时器正常倒计时，其余计时器仅记录终止时间，接在链表的尾部。

- 收到Ack
  - 检查包的完整性
  - 更新next_ack和nbuffered信息，并将waiting buffer中后续的包发出。并更新相应的计时器。

- Timeout

  如果出现超时，Sender对超时的包进行重传，并更新计时器。

**Receiver逻辑**

- 收到package 并构建Message向上层转发

  - 检查包的完整性（注意对数据范围的检查和类型转换）
  - 解包，提取出对应的数据
  - 用expected_seq(表示等待的下一个包)来维护接收端的sliding window。已经送达的数据缓存到 message_slices这个buffer中，直到收到最后一个包才将当前buffer中的包组成完整的message发送给network层。而提前送达的包被缓存到recv_buffer中，只有当前面的包都收到后，才会将recv_buffer中的包转移到messgae_slices这个buffer中。
  - 收到任何一个包后，都想sender发送expect_seq - 1的ack。即当前接收端sliding window的所在位置。

  

  

### usage

- make 
- ./rdt_sim 1000 0.1 100 0.15 0.15 0.15 0
- due to the limitation of checksumming. still possible to err

### future
- may introduce Nak and  implement selective repeat later.

