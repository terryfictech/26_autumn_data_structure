#from teacher's ppt

# import time
# n = int(input("请输入整数n: "))
# myrange=range(1,n)
# mylist=[]
# #方法一
# start = time.time()
# for i in myrange:
#   mylist.append(i)
# end = time.time()
# print(f"方法一插入数据耗时: {(end - start) * 1000:.6f} 毫秒")
# start = time.time()
# mylist.sort(reverse=True)
# end = time.time()
# print(f"方法一排序耗时: {(end - start) * 1000:.6f} 毫秒")
# #方法二
# start = time.time()
# for i in myrange:
#   mylist.insert(0,i)
# end = time.time()
# print(f"方法二耗时: {(end - start) * 1000:.6f} 毫秒")

#solution

import time
import matplotlib.pyplot as plt


# 测试的数据规模
sizes = [10_000, 100_000, 1_000_000]

method1_times = []
method2_times = []


for n in sizes:

    print(f"\n========== n = {n:,} ==========")

    # =================================
    # 方法一：append() + sort()
    # =================================

    mylist = []

    start = time.perf_counter()

    # 插入
    for i in range(1, n + 1):
        mylist.append(i)

    insert_end = time.perf_counter()

    # 排序
    mylist.sort(reverse=True)

    end = time.perf_counter()

    append_time = insert_end - start
    sort_time = end - insert_end
    total_time = end - start

    method1_times.append(total_time)

    print(f"方法一 append 插入时间：{append_time:.6f} 秒")
    print(f"方法一 sort 排序时间：   {sort_time:.6f} 秒")
    print(f"方法一总时间：           {total_time:.6f} 秒")


    # =================================
    # 方法二：insert(0, i)
    # =================================

    mylist = []

    start = time.perf_counter()

    for i in range(1, n + 1):
        mylist.insert(0, i)

    end = time.perf_counter()

    insert_time = end - start

    method2_times.append(insert_time)

    print(f"方法二总时间：           {insert_time:.6f} 秒")


# =================================
# 输出最终结果
# =================================

print("\n\n========== 最终结果 ==========")

print("数据量\t\t方法一\t\t方法二")

for i in range(len(sizes)):
    print(
        f"{sizes[i]:,}\t\t"
        f"{method1_times[i]:.6f}s\t\t"
        f"{method2_times[i]:.6f}s"
    )


# =================================
# 绘制时间曲线
# =================================

plt.figure(figsize=(10, 6))

plt.plot(
    sizes,
    method1_times,
    marker="o",
    label="append() + sort()"
)

plt.plot(
    sizes,
    method2_times,
    marker="o",
    label="insert(0, i)"
)

plt.xlabel("Number of elements")
plt.ylabel("Time (seconds)")
plt.title("Comparison of Two Methods")

plt.legend()
plt.grid(True)

# 横坐标使用科学计数法
plt.ticklabel_format(style="sci", axis="x", scilimits=(0, 0))

plt.show()