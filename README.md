Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)

Disributed under the Boost Software Licence, Version 1.0. (See accompanying file
[LICENCE_1_0.txt](LICENCE_1_0.txt) or a copy at 
[http://www.boost.org/LICENSE_1_0.txt]( http://www.boost.org/LICENSE_1_0.txt))

This repo contains a number of examples that I have written in response to being asked 
questions about Boost.Beast and Boost.Asio.

This repository makes NO CLAIMS WHATSOEVER to represent the views of the Beast team,
or the views of Beast's author. In particular, the coding style used here is my own, 
totally arbitrary and is not endorsed by the Beast team or used in the Beast library itself.

I use a `.clang-format` file and take what I am given - because I can't be bothered to 
have code style arguments with other people or myself. My personal view is that the ends 
justify the means. Correct code is code that works, delivers required functionality, 
does not leak resources, shuts down cleanly and never segfaults. If you want to fork
the repo, prettify the code and tinker with it for performance gains, please fill your
boots.

I may even use the occasional `goto`. If that's going to make you spit your 
dummy<span id="a1">[¹](#1)</span> out, you can leave the room now.

The examples are provided in response to questions that do not already have a clear 
answer in the official documentation, or perhaps the documentation does cover it but
people want to see an example because their level of pre-existing expertise is not 
sufficient to understand what it being presented to them in the documentation 
(this is common).
 
**The Beast documentation clearly states that users of Beast should already be proficient
with Asio.**

This is reasonable as it keeps the human cost of support in the Beast maintenance team 
low.

However in reality, there is a chicken and egg scenario: Beast provides people a reason
to use Asio. But it demands that people first learn Asio. Asio is hard to learn, even harder 
without a motivating use case. In today's HTTP/Websocket-centric world, Beast is often the 
motivating factor.

Asio is hard to learn because its documentation expects a certain level of knowledge
in computer science, asynchronous messaging and underlying network IO. I've been using
Asio for some time in production projects. My first attempts were wrong, because I
didn't understand (and didn't bother to read up properly) on the meanings of 
_Execution Context_, _Implicit Strand_, _Completion Handler_ and _Completion Token_. 
I just wanted to dive in and make things happen. 

I expect since you're here, you do too.

This is why this repository exists. To provide annotated examples which hopefully 
explain why things are done the way they are (at least by me).

# Common Misconceptions

## Completion Handler

Many people think that _Completion Handler_ means callback.

*This is totally false.*

OK, it's not _totally_ false. A _Completion Handler_ is a kind of callback, in the same way
that an elephant is a kind of animal. All elephants are indeed animals, but not all animals
have elongated trunks, long memories and are being hunted to extinction by psychopathic 
hairless apes for their solidified whiskers.

It is better to think of a _Completion Handler_, in the context of the last argument provided
to an _Asynchronous Initiating Function_ as a form of _Future_<span id="a2">[²](#2)</span>. 
It is a guarantee that some code will execute in the future by being _Invoked_ by an 
_Executor_. Once. This means something different to merely _Callback_.

The code will be invoked on its _Associated Executor_, which is either:

* The default executor associated with the IO object that has promised to invoke your
completion handler, or
* The executor you bound to your completion handler function with 
`asio::bind_executor`, or
* The executor associated with the current coroutine (the one you specified in the 
call to `asio::co_spawn` if you supplied the `asio::use_awaitable` 
_Completion Token_, or
* Totally irrelevant if you supplied the _Completion Token_ `asio::use_future`

## Completion Token

You might be tempted to think that a _Completion Token_ means the same thing as a
_Completion Handler_. 

**You would be wrong.**

And this time, no qualification required, wrong. 

A _Completion Token_ is different from a _Completion Handler_ in the same way that a high 
performance nickel-based superalloy jet turbine blade is different from a pet rat.

A _Completion Handler_ is a function object that will be _Invoked_ on the _Associated
Executor_ in response to the completion of an _Asynchronous Operation_.

A _Completion Token_ describes to Asio how to rewrite the _Initiating Function_ in order
to:
* Ensure that the Asynchronous Operation is initiated correctly. 
* Ensure that the correct _Completion Handler_ is manufactured and passed to the asynchronous 
operation for later execution.
* Ensure that the correct _Result Type_ is returned to the caller, so that the caller can
await the _Completion_ of the _Asynchronous Operation_ in the appropriate way.

For example, the _Completion Handler_ produced by the _Completion Token_ `asio::use_awaitable`,
actually invokes code to resume the current coroutine upon operation completion.

The _Completion Handler_ produced by the _Completion Token_ `asio::use_future`
actually invokes code to supply a value or an error to the `std::promise` backing
the `std::future` you received as a result of calling the _Initiating Function_, e.g.: 
```c++
    auto f = my_timer.async_wait(asio::use_future);
```

"But I've always just supplied a lambda"...

Right. Because if you supply an _Completion Handler_ where a _Completion Token_ is specified
in the documents, a minimal transformation on your handler is made, to ensure that:
* The initiating function returns `void`
* All steps of the _Asynchronous Operation_ make progress by being invoked by the IO object's
_Default Executor_.
* the supplied lambda will be _Invoked_ through the io object's _Default excecutor_ - once.

Examples:

```c++
using namespace asio;

// This is an execution context, not an executor. The only thing you should ever do 
// with this is call run() on it. Do not move it, store it in a shared pointer or
// any other daft thing. create it in main() and leave it there.

auto ioc = io_context();

// This is an Executor. You can copy this and pass it around. It is essentially
// a cheap handle to the io_context. 

auto e = ioc.get_executor();
   
// This timer's Default Executor is e
auto t = system_timer(e);

// as is the socket's
auto s = ip::tcp::socket(e);

// f is a std::future<void>. It will throw if the timer errors out or is cancelled.
auto f = t.async_wait(use_future);

// Returns an awaitable, which you must co_await. When it _resumes_ your coroutine,
// either `bytes` will contain the number of bytes you actually read or an exception
// (of type std::system_error) will be thrown.

std::vector mem(128);
auto bytes = co_await s.async_read_some(buffer(mem), use_awaitable);

async_wait(t, [](error_code ec){
    // This is a Completion Handler. Think of it as a FUTURE. It will happen exactly once.
    // It will happen by being called by executor e.
});
```

## "All steps of the Asynchronous Operation make progress by being invoked by the IO object's Default Executor"

Wait, what?

consider:

```c++
auto bytes = co_await beast::http::async_read(sock, buffer, parser, asio::use_awaitable);
```

In the above asynchronous operation, how many actual asynchronous reads will be performed before the
macro operation of consuming an entire HTTP request _completes_? It's not knowable until the final
completion handler is invoked and (in this case), the current coroutine resumes execution.

So what?

Well, every time the sub-operations of which this operation is composed need to acquire or parse more
data, they are going to touch the socket, or the buffer, or the parser, or all three.

On which thread should they do this?

They will make progress within the context of the _Completion Handler_'s _Associated Executor_. In this
case, the same executor on which the calling coroutine make progress. 

Why is that important?

Because the 'reader' coroutine may not be the only one touching the socket. There may well be a 'writer'
coroutine in progress too. And maybe a 'timer' coroutine which at some point might call `cancel` on
our socket. Asio (and Beast) IO Objects are not thread-safe by design. That call to `cancel` or 
`async_write` had better happen on the _same implicit or explicit strand_ of execution. 

The reasons they are not thread safe are:

1. Thread safety would unduly pessimise single-threaded programs or programs that create 
_Implicit Strands_. (i.e. they would be slower).

2. IO Objects and in particular complex asynchronous streams and operations like the above function
would be harder to maintain.

3. _Intermediate Completion Handlers_ would need to each maintain some kind of mutex before touching
any data they had access to. (see 1 and 2)

In simple terms, it means that we can protect the entire "connection object" or "connection state" with 
a single `strand` and never have to worry about mutexes ever again.

Asio is able to achieve extremely high _jitterless_ throughput because of its reliance of execution
of code within executors. This method of execution provides very high confidence of fair sharing of
cpu time. This is particularly important in high capacity server environments, but is
also useful nice to have in clients.

# Contributing

You are welcome to contribute a PR. If it brings new dependencies, please make sure 
they build properly using CMake FetchContent. User experience of this repo should be
as trouble-free as possible.

# Commenting and Raising Issues

Please feel free to comment and raise issues or contact me in slack once the procedure
below has been followed.

Issues raised over code layout or style issues that do not affect program correctness
or efficiency will simply be ignored and closed.

# Getting Help

Here is the procedure for getting help:

Does the thing I'm doing resemble in any way one of the examples here?

if yes, 

&nbsp;&nbsp;&nbsp;&nbsp;copy the example and tinker from there.

&nbsp;&nbsp;&nbsp;&nbsp;if you get really stuck, goto [Slack](#Slack) 

if no,

&nbsp;&nbsp;&nbsp;&nbsp;submit a PR with a link to compilable code on 
[Goldbolt](https://godbolt.org/) demonstrating what you have already tried.

End

<span id="Slack">[Link to Slack Invitation](https://cppalliance.org/slack/)</span> Join 
the `#beast` channel and politely as a question there. Expect to be flamed if the answer
is in the docs.

# Social Responsibility Policy

I'm not a socially reponsible person. Why I'm allowed out by myself it is a mystery to
me. If you feel insulted or triggered by anything I say to you, you should probably
grow up and stop being a baby. Even better, think of a more offensive retort. If you
make me laugh, I may even respect you.

In code, there is correct (program functions) and there is wrong (program does not 
function, or functions by luck). Your feelings are irrelevant.

# Environmental Responsibility Policy

If you are cruel to animals or engaged in activities that reduce the natural beauty
of the World, then we probably won't get on. I'd keep that quiet if I were you, or
better yet, change your ways and become a good person.
 
# Footnotes
 <span id="1">¹</span> Americans might use the word 'comforter' here...[⏎](#a1)<br>
 
 <span id="2">²</span> Anyone out there who can find an official text on computer science 
 that disproves my claim that a completion handler is a form of future, please step forward.
 I enjoy a good argument, particularly one that is well presented and supported by factual
 documentation.[⏎](#a2)<br>
