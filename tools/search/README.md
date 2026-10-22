# 离线拼音搜索

运行时为纯C静态字表，无Python、联网或系统转换API。入口include/search/pinyin.h、src/search/pinyin.c；生成数据include/search/pinyin_data.h，44,350个汉字。

数据来自Unicode 17.0.0 Unihan.zip中kMandarin/kHanyuPinyin，许可见LICENSE-UNICODE.txt；来源和ZIP SHA256记录于pinyin-source.json。无声调拼音，ü映射v并接受u；收录多读音，但不进行词义消歧。字表并非所有汉字的完整读音词典。

更新/重建：从元数据里的Unicode官方URL取得固定ZIP和license.txt，分别保存build/pinyin/Unihan-17.0.0.zip、build/pinyin/LICENSE-UNICODE.txt，再运行tools/search/generate_pinyin.py。生成器核对固定ZIP SHA256，输出随源码提交；普通构建不需要重新下载。

匹配：中文原文、全拼、首字母、大小写不敏感ASCII、混合输入。拼音可从字的音节起点连续匹配，允许末尾不完整音节，允许全拼/首字母混用；不是编辑距离/错别字纠正。空格、单引号、连字符在比较中忽略，ISBN可不输入连字符。读音不在表中的字仍可按原文匹配，不跨越未识别标点。

查询长度最多511字节，检查UTF8，不截断溢出；动态匹配状态有界，避免展开多音字所有字符串组合。二分查字表、自写遍历，不调用禁用转换/排序函数。

make test-search验证22项，make test-application验证实际持久化目录、读者、关联借阅元数据的搜索。角色过滤在关联搜索前执行；hash与salt列始终排除。
