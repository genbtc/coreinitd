# **Design: Minimal `Accept=yes` Support for coreinitd**

---

## **Background**

In systemd, a `.socket` unit with `Accept=yes` causes a new `.service` process to be started for **each incoming connection**, 
receiving only the accepted client socket (not the listening socket). 
This model mimics `inetd`, but is safer and more controlled in systemd's implementation.

`coreinitd` will adopt a minimalist version of this pattern with modern safety measures.

---

## **Behavior**

If a `.socket` unit has:

```ini
[Socket]
Accept=yes
```

Then:

1. The `.socket` unit binds and listens as usual.
2. `coreinitd` calls `accept()` when a connection arrives.
3. A new `.service` unit is spawned with:

   * One client socket FD passed as FD 3
   * `LISTEN_PID=<child_pid>`
   * `LISTEN_FDS=1`
4. The service is expected to handle that single connection and then exit.

---

## **Implementation Plan**

### **Socket Binding (`socket_activation.c`)**

* Parse `Accept=` key (default to `no`)
* For `Accept=yes`, mark unit and store listen FD
* In the event loop:

  * Instead of treating the socket as passive input (like `Accept=no`)
  * Set up a `sd_event_add_io()` watcher on the socket FD
  * On read-ready, call `accept()` once
  * Pass returned FD to a new `.service` instance

---

### **Service Fork (`accept_service_launcher()`)**

In response to new connection:

1. `fork()`
2. In child:

   * Duplicate accepted socket FD to FD 3
   * Set environment:

     * `LISTEN_FDS=1`
     * `LISTEN_PID=<child_pid>`
   * `execv()` the service’s `ExecStart` binary
3. Parent closes the accepted FD

---

## **Best Practices**

* **No double-fork**: the child runs foreground, supervised
* **Set CLOEXEC=0** only on passed FD 3
* **Do not pass listening socket** to the service
* **One FD per process** only
* **Rate-limiting (future)**: prevent fork bombs via backlog or accept interval

---

## **Example**

**`foo.socket`**

```ini
[Socket]
ListenStream=/run/foo.sock
Accept=yes
```

**`foo@.service`**

```ini
[Service]
ExecStart=/usr/bin/foo-handler
```

On connect, `/usr/bin/foo-handler` receives client FD 3, and handles it.

---

## **Limitations**

* No `@`-templating in `.service` names yet
* No max connection rate limiting
* No cleanup on handler crash

---

## **Future Work**

* Integrate `@instance` substitution into service names
* Graceful failure handling
* Per-socket rate limits or queuing

---

Let me know when you’re ready to implement this, or want to adjust the design!
