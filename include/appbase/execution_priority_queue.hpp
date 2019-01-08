#pragma once
#include <boost/asio.hpp>

#include <queue>

//#define DEBUG_PRIORITY_QUEUE
#ifdef DEBUG_PRIORITY_QUEUE
#include <iostream>
#include <string>
#include <chrono>
#endif

namespace appbase {
// adapted from: https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/example/cpp11/invocation/prioritised_handlers.cpp

struct priority {
   static constexpr int high = 100;
   static constexpr int medium = 50;
   static constexpr int low = 10;
};

class execution_priority_queue : public boost::asio::execution_context
{
public:

   template <typename Function>
   void add(int priority, Function function)
   {
      std::unique_ptr<queued_handler_base> handler(new queued_handler<Function>(priority, std::move(function)));

      handlers_.push(std::move(handler));
   }

   void execute_all()
   {
      while (!handlers_.empty())
      {
         handlers_.top()->execute();
         handlers_.pop();
      }
   }

   bool execute_highest()
   {
      // execute all with priority of HIGH or higher
      while( !handlers_.empty() )
      {
         const auto& top = handlers_.top();
         const int priority = top->priority();
         top->execute();
         handlers_.pop();
         if( priority < priority::high ) {
            // execute only one after executing all of high
            break;
         }
      }

      return !handlers_.empty();
   }

   void execute_high() {
      // execute all with priority of HIGH or higher
      while( !handlers_.empty() )
      {
         const auto& top = handlers_.top();
         const int priority = top->priority();
         if( priority < priority::high ) {
            break;
         }
         top->execute();
         handlers_.pop();
      }
   }

   class executor
   {
   public:
#ifdef DEBUG_PRIORITY_QUEUE
      executor(execution_priority_queue& q, int p, std::string&& debug_desc)
            : context_(q), priority_(p), debug_desc_(std::move(debug_desc))
      {
      }
#else
      executor(execution_priority_queue& q, int p)
            : context_(q), priority_(p)
      {
      }
#endif

      execution_priority_queue& context() const noexcept
      {
         return context_;
      }

      template <typename Function, typename Allocator>
      void dispatch(Function f, const Allocator&) const
      {
         context_.add(priority_, std::move(f));
      }

      template <typename Function, typename Allocator>
      void post(Function f, const Allocator&) const
      {
         context_.add(priority_, std::move(f));
      }

      template <typename Function, typename Allocator>
      void defer(Function f, const Allocator&) const
      {
         context_.add(priority_, std::move(f));
      }
      
#ifdef DEBUG_PRIORITY_QUEUE
      static std::string get_current_time() {
         using namespace std;
         using namespace std::chrono;
         auto tp = std::chrono::system_clock::now();

         auto ttime_t = system_clock::to_time_t(tp);
         auto tp_sec = system_clock::from_time_t(ttime_t);
         milliseconds ms = duration_cast<milliseconds>(tp - tp_sec);

         std::tm * ttm = localtime(&ttime_t);

         char date_time_format[] = "%Y.%m.%d-%H.%M.%S";

         char time_str[] = "yyyy.mm.dd.HH-MM.SS.fff";

         strftime(time_str, strlen(time_str), date_time_format, ttm);

         string result(time_str);
         result.append(".");
         long val = ms.count();
         if( val < 100 ) result += "0";
         if( val < 10 ) result += "0";
         result.append( to_string( val ));
         return result;
      } 
#endif

      void on_work_started() const noexcept {
#ifdef DEBUG_PRIORITY_QUEUE
         std::cerr << "debug " << get_current_time() << " " << debug_desc_ << " started" << std::endl;
#endif
      }
      void on_work_finished() const noexcept {
#ifdef DEBUG_PRIORITY_QUEUE
         std::cerr << "debug " << get_current_time() << " " << debug_desc_ << " finished" << std::endl;
#endif
      }

      bool operator==(const executor& other) const noexcept
      {
         return &context_ == &other.context_ && priority_ == other.priority_;
      }

      bool operator!=(const executor& other) const noexcept
      {
         return !operator==(other);
      }

   private:
      execution_priority_queue& context_;
      int priority_;
#ifdef DEBUG_PRIORITY_QUEUE
      std::string debug_desc_;
#endif
   };

   template <typename Function>
   boost::asio::executor_binder<Function, executor>
   wrap(int priority, Function func, const char* file, int line, const char* func_name)
   {
#ifdef DEBUG_PRIORITY_QUEUE
      std::string desc = file;
      desc += ":"; desc += std::to_string(line);
      desc += " "; desc += func_name;
      return boost::asio::bind_executor( executor(*this, priority, std::move(desc)), std::move(func) );
#else
      return boost::asio::bind_executor( executor(*this, priority), std::move(func) );
#endif
   }

private:
   class queued_handler_base
   {
   public:
      queued_handler_base(int p)
            : priority_(p)
      {
      }

      virtual ~queued_handler_base()
      {
      }

      virtual void execute() = 0;

      int priority() const { return priority_; }

      friend bool operator<(const std::unique_ptr<queued_handler_base>& a,
                            const std::unique_ptr<queued_handler_base>& b) noexcept
      {
         return a->priority_ < b->priority_;
      }

   private:
      int priority_;
   };

   template <typename Function>
   class queued_handler : public queued_handler_base
   {
   public:
      queued_handler(int p, Function f)
            : queued_handler_base(p), function_(std::move(f))
      {
      }

      void execute() override
      {
         function_();
      }

   private:
      Function function_;
   };

   std::priority_queue<std::unique_ptr<queued_handler_base>, std::deque<std::unique_ptr<queued_handler_base>>> handlers_;
};

} // appbase
