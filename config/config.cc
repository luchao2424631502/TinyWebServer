#include "config.h"

Config::Config()
{

	/*
	 * -p 
	 * */
	PORT = 8080;

	/*
	 * 日志写入方式 -l 
	 * 同步=0,异步=1
	 */
	LOGWrite = 0;

	/*
	 * 触发组合模式: -m
	 * listenfd LT + connfd LT 0
	 * 1: LT + ET 
	 * 2: ET + LT 
	 * 3: ET + ET
	 */
	TRIGMode = 0;

	//listenfd触发模式,如上 LT 
	LISTENTrigmode = 0;

	//connfd触发模式 LT 
	CONNTrigmode = 0;

	/*优雅关闭连接, 
	 * 0: shutdown
	 * 1: open
	 */
	OPT_LINGER = 0;

	/*数据库连接池数量 
	 * -s =8
	 */
	sql_num = 8;

	/*线程池内线程数量, =8默认 
	 * -t 
	 */
	thread_num = 8;

	/*关闭日志 
	 * 开启=0,关闭=1
	 * -c 
	 */
	close_log = 0;

	/*并发模型 
	 * 0: proactor
	 * 1: reactor
	 */
	actor_model = 0;
}

//解析启动的命令行  extern char* optarg
void Config::parse_arg(int argc,char* argv[])
{
   int opt;
   //一个字符代表一个选项,加:表示选项带一个参数
   const char* str = "p:l:m:o:s:t:c:a:";
   while((opt = getopt(argc,argv,str)) != -1)
   {
       switch(opt)
       {
        case 'p':
            {
                PORT = atoi(optarg);
                break;
            }
        case 'l'://日志写入模式
            {
                LOGWrite = atoi(optarg);
                break;
            }
        case 'm'://组合触发模式
            {
                TRIGMode = atoi(optarg);
                break;
            }
        case 'o'://是否优雅关闭连接
            {
                OPT_LINGER = atoi(optarg);
                break;
            }
        case 's'://sql池数量
            {
                sql_num = atoi(optarg);
                break;
            }
        case 't'://线程池数量
            {
                thread_num = atoi(optarg);
                break;
            }
        case 'c'://是否开启日志系统
            {
                close_log = atoi(optarg);
                break;
            }
        case 'a'://并发模式
            {
                actor_model = atoi(optarg);
                break;
            }
        default:
            break;
       }
   }
}

void Config::Print()
{
    std::cout << std::string("thread_num: ") << thread_num << std::endl; 
}
