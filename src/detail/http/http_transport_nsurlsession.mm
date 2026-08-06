// Default HTTP transport for macOS/iOS. Uses NSURLSession so plugins pick
// up the OS's system proxy configuration, TLS trust store, and (on macOS)
// App Sandbox network entitlements for free, rather than vendoring a TLS
// stack. `perform` blocks the calling (worker) thread on a semaphore since
// the HttpTransport contract is synchronous — NSURLSession's own delivery
// queue does the actual I/O off-thread either way.
#import <Foundation/Foundation.h>

#include <dispatch/dispatch.h>

#include <memory>

#include "licensr/http_transport.h"

namespace {

class NSURLSessionHttpTransport : public licensr::HttpTransport {
 public:
  licensr::HttpResponse perform(const licensr::HttpRequest& request) override {
    @autoreleasepool {
      __block licensr::HttpResponse response;

      NSString* url_string = [NSString stringWithUTF8String:request.url.c_str()];
      NSURL* url = [NSURL URLWithString:url_string];
      if (url == nil) {
        response.error_message = "invalid URL";
        return response;
      }

      NSMutableURLRequest* ns_request = [NSMutableURLRequest requestWithURL:url];
      ns_request.HTTPMethod = request.method == licensr::HttpMethod::kPost ? @"POST" : @"GET";
      if (request.timeout_ms > 0) {
        ns_request.timeoutInterval = static_cast<NSTimeInterval>(request.timeout_ms) / 1000.0;
      }
      for (const auto& [name, value] : request.headers) {
        [ns_request setValue:[NSString stringWithUTF8String:value.c_str()]
            forHTTPHeaderField:[NSString stringWithUTF8String:name.c_str()]];
      }
      if (request.method == licensr::HttpMethod::kPost) {
        ns_request.HTTPBody = [NSData dataWithBytes:request.body.data() length:request.body.size()];
      }

      dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
      NSURLSessionConfiguration* config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
      NSURLSession* session = [NSURLSession sessionWithConfiguration:config];

      NSURLSessionDataTask* task =
          [session dataTaskWithRequest:ns_request
                     completionHandler:^(NSData* _Nullable data, NSURLResponse* _Nullable ns_response,
                                         NSError* _Nullable error) {
                       if (error != nil) {
                         response.status = 0;
                         response.error_message = [error.localizedDescription UTF8String];
                       } else if (auto* http_response = (NSHTTPURLResponse*)ns_response) {
                         response.status = static_cast<int>(http_response.statusCode);
                         if (data != nil && data.length > 0) {
                           response.body.assign(static_cast<const char*>(data.bytes), data.length);
                         }
                         [http_response.allHeaderFields
                             enumerateKeysAndObjectsUsingBlock:^(NSString* key, NSString* value, BOOL* stop) {
                               response.headers.emplace_back(key.UTF8String, value.UTF8String);
                               (void)stop;
                             }];
                       }
                       dispatch_semaphore_signal(semaphore);
                     }];
      [task resume];
      dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

      return response;
    }
  }
};

}  // namespace

namespace licensr {

std::shared_ptr<HttpTransport> make_default_http_transport() { return std::make_shared<NSURLSessionHttpTransport>(); }

}  // namespace licensr
