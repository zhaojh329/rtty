贡献代码
================================================================================

如果你想为[rtty](https://github.com/zhaojh329/rtty)贡献代码, 请按照如下步骤:

1. 点击fork按钮:

    ![fork](http://oi58.tinypic.com/jj2trm.jpg)

2. 从你的github账户克隆仓库代码:

    ```
    git clone https://github.com/你的github账户/rtty.git
    ```

3. 创建一个新的分支:

    ```
    git checkout -b "rtty-1-fix"
    ```
    你可以使用一个你想要的分支名称。

4. 修改代码

5. 提交代码并推送到服务器，然后从Github提交pull request。

    git commit --signoff
    git push origin rtty-1-fix

6. 等待审查，如果被接受，你的修改将会被合并到主分支。

**注意**

请不要忘记更新你的fork。当你修改代码时，主分支的内容可能已被修改，因为其他用户提交的pull request被合并，
这就会产生代码冲突。这就是为什么在你每次修改前都要在你的主分支上进行rebase操作

谢谢.
