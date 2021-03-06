/* Copyright 2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Expose glibc-specific EAI_* error codes. Needs to be defined before we
 * include any headers.
 */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <errno.h>
#include <stddef.h> /* NULL */
#include <stdlib.h>
#include <string.h>

#include <uv.h>

/* EAI_* constants. */
//#if !defined(__NUTTX__)
#include <netdb.h>
//#endif


int uv__getaddrinfo_translate_error(int sys_err) {
  switch (sys_err) {
  case 0: return 0;
#if defined(EAI_ADDRFAMILY)
  case EAI_ADDRFAMILY: return UV_EAI_ADDRFAMILY;
#endif
#if defined(EAI_AGAIN)
  case EAI_AGAIN: return UV_EAI_AGAIN;
#endif
#if defined(EAI_BADFLAGS)
  case EAI_BADFLAGS: return UV_EAI_BADFLAGS;
#endif
#if defined(EAI_BADHINTS)
  case EAI_BADHINTS: return UV_EAI_BADHINTS;
#endif
#if defined(EAI_CANCELED)
  case EAI_CANCELED: return UV_EAI_CANCELED;
#endif
#if defined(EAI_FAIL)
  case EAI_FAIL: return UV_EAI_FAIL;
#endif
#if defined(EAI_FAMILY)
  case EAI_FAMILY: return UV_EAI_FAMILY;
#endif
#if defined(EAI_MEMORY)
  case EAI_MEMORY: return UV_EAI_MEMORY;
#endif
#if defined(EAI_NODATA)
  case EAI_NODATA: return UV_EAI_NODATA;
#endif
#if defined(EAI_NONAME)
# if !defined(EAI_NODATA) || EAI_NODATA != EAI_NONAME
  case EAI_NONAME: return UV_EAI_NONAME;
# endif
#endif
#if defined(EAI_OVERFLOW)
  case EAI_OVERFLOW: return UV_EAI_OVERFLOW;
#endif
#if defined(EAI_PROTOCOL)
  case EAI_PROTOCOL: return UV_EAI_PROTOCOL;
#endif
#if defined(EAI_SERVICE)
  case EAI_SERVICE: return UV_EAI_SERVICE;
#endif
#if defined(EAI_SOCKTYPE)
  case EAI_SOCKTYPE: return UV_EAI_SOCKTYPE;
#endif
#if defined(EAI_SYSTEM)
  case EAI_SYSTEM: return -errno;
#endif
  }
  assert(!"unknown EAI_* error code");
  ABORT();
  return 0;  /* Pacify compiler. */
}


static void uv__getaddrinfo_work(struct uv__work* w) {
  uv_getaddrinfo_t* req;
  int err;

  req = container_of(w, uv_getaddrinfo_t, work_req);
#if defined(__NUTTX__)
  err = 0;
#else
  /* Only IPv4 is supported now. (Not support IPv6.)*/
  if (req->hints) {
    req->hints->ai_family = AF_INET;
  }
  err = getaddrinfo(req->hostname, req->service, req->hints, &req->addrinfo);
#endif
  req->retcode = uv__getaddrinfo_translate_error(err);
}


static void uv__getaddrinfo_done(struct uv__work* w, int status) {
  uv_getaddrinfo_t* req;

  req = container_of(w, uv_getaddrinfo_t, work_req);
  uv__req_unregister(req->loop, req);

  /* See initialization in uv_getaddrinfo(). */
  if (req->hints) {
    free(req->hints);
  } else if (req->service) {
    free(req->service);
  } else if (req->hostname) {
    free(req->hostname);
  } else {
    assert(0);
  }

  req->hints = NULL;
  req->service = NULL;
  req->hostname = NULL;

  if (status == -ECANCELED) {
    assert(req->retcode == 0);
    req->retcode = UV_EAI_CANCELED;
  }

  if (req->cb) {
    req->cb(req, req->retcode, req->addrinfo);
  }
}


int uv_getaddrinfo(uv_loop_t* loop,
                   uv_getaddrinfo_t* req,
                   uv_getaddrinfo_cb cb,
                   const char* hostname,
                   const char* service,
                   const struct addrinfo* hints) {
  size_t hostname_len;
  size_t service_len;
  size_t hints_len;
  size_t len;
  char* buf;

  if (req == NULL || (hostname == NULL && service == NULL)) {
    return -EINVAL;
  }

  hostname_len = hostname ? strlen(hostname) + 1 : 0;
  service_len = service ? strlen(service) + 1 : 0;
  hints_len = hints ? sizeof(*hints) : 0;
  buf = (char*)malloc(hostname_len + service_len + hints_len);

  if (buf == NULL) {
    return -ENOMEM;
  }

  uv__req_init(loop, req, UV_GETADDRINFO);
  req->loop = loop;
  req->cb = cb;
  req->addrinfo = NULL;
  req->hints = NULL;
  req->service = NULL;
  req->hostname = NULL;
  req->retcode = 0;

  /* order matters, see uv_getaddrinfo_done() */
  len = 0;

  if (hints) {
    req->hints = (struct addrinfo*)memcpy(buf + len, hints, sizeof(*hints));
    len += sizeof(*hints);
  }

  if (service) {
    req->service = (char*)memcpy(buf + len, service, service_len);
    len += service_len;
  }

  if (hostname) {
    req->hostname = (char*)memcpy(buf + len, hostname, hostname_len);
  }

  if (cb) {
    uv__work_submit(loop,
                    &req->work_req,
                    uv__getaddrinfo_work,
                    uv__getaddrinfo_done);
    return 0;
  } else {
    uv__getaddrinfo_work(&req->work_req);
    uv__getaddrinfo_done(&req->work_req, 0);
    return req->retcode;
  }
}


void uv_freeaddrinfo(struct addrinfo* ai) {
  if (ai) {
#if defined(__NUTTX__)

#else
    freeaddrinfo(ai);
#endif
  }
}
