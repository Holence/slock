`Fn+L`键会让系统去suspend，需要创建一个`slock.service`，其在`suspend.target`之后运行（也就是电脑被唤醒后运行）

```sh
sudo cp ./slock.service /etc/systemd/system/slock.service
sudo systemctl enable slock.service
sudo systemctl daemon-reload
```

> 1. Any service that you create (i.e. one that is not installed by a package, for example) should go in `/etc/systemd/system/`
> 2. After creating or changing the service, you must run `systemctl daemon-reload`.
> 3. Normally, if your service is failing, there are two main ways of troubleshooting it (in my opinion):
>   - Run `journalctl -u [your_service]` to show logs for the service. This may or may not show something useful.
>   - Attempt to run whatever is specified as `ExecStart=` manually as the user specified by `User=`. By default and if not specified, this will be root.

> 比如，如果设置为`Type=forking`会导致电脑被唤醒后、slock静置一段时间后，slock被杀死，log如下：
>```
>systemd[1]: Starting slock.service - User suspend actions...
>systemd[1]: slock.service: start operation timed out. Terminating.
>systemd[1]: slock.service: Failed with result 'timeout'.
>systemd[1]: Failed to start slock.service - User suspend actions.
>```
>去问chatgpt，就会知道不应该用`Type=forking`，而应该用默认的`Type=simple`