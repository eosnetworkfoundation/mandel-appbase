#pragma once

//clashes with BOOST PP and Some Applications
#pragma push_macro("N")
#undef N

#include <boost/signals2.hpp>
#include <boost/exception/diagnostic_information.hpp>

namespace appbase {

   using erased_method_ptr = std::unique_ptr<void, void(*)(void*)>;

   namespace impl {
      template<typename Ret, typename ... Args>
      struct dispatch_policy_helper_impl {
         using result_type = Ret;
      };

      template<typename FunctionSig>
      struct method_traits;

      template<typename Ret, typename ...Args>
      struct method_traits<Ret(Args...)> {
         using result_type = typename dispatch_policy_helper_impl<Ret, Args...>::result_type;
         using args_tuple_type = std::tuple<Args...>;

      };
   }

   /**
    * Basic DispatchPolicy that will try providers sequentially until one succeeds
    * without throwing an exception.  that result becomes the result of the method
    */
   template<typename FunctionSig>
   struct first_success_policy {
      using result_type = typename impl::method_traits<FunctionSig>::result_type;
      std::string err;

      /**
       * Iterate through the providers, calling (dereferencing) each
       * if the provider throws, then store then try the next provider
       * if none succeed throw an error with the aggregated error descriptions
       *
       * @tparam InputIterator
       * @param first
       * @param last
       * @return
       */
      template<typename InputIterator>
      result_type operator()(InputIterator first, InputIterator last) {
         while (first != last) {
            try {
               return *first; // de-referencing the iterator causes the provider to run
            } catch (...) {
               if (!err.empty()) {
                  err += "\",\"";
               }

               err += boost::current_exception_diagnostic_information();
            }

            ++first;
         }

         throw std::length_error(std::string("No Result Available, All providers returned exceptions[") + err + "]");
      }
   };

   /**
    * A method is a loosely linked application level function.
    * Callers can grab a method and call it
    * Providers can grab a method and register themselves
    *
    * This removes the need to tightly couple different plugins in the application.
    *
    * @tparam FunctionSig - the signature of the method (eg void(int, int))
    * @tparam DispatchPolicy - the policy for dispatching this method
    */
   template<typename FunctionSig, typename DispatchPolicy>
   class method final {
      public:
         using traits = impl::method_traits<FunctionSig>;
         using args_tuple_type = typename traits::args_tuple_type;
         using result_type = typename traits::result_type;

         /**
          * Type that represents a registered provider for a method allowing
          * for ownership via RAII and also explicit unregistered actions
          */
         class handle {
            public:
               ~handle() {
                  unregister();
               }

               /**
                * Explicitly unregister a provider for this channel
                * of this object expires
                */
               void unregister() {
                  if (_handle.connected()) {
                     _handle.disconnect();
                  }
               }

               // This handle can be constructed and moved
               handle() = default;
               handle(handle&&) = default;
               handle& operator= (handle&& rhs) = default;

               // dont allow copying since this protects the resource
               handle(const handle& ) = delete;
               handle& operator= (const handle& ) = delete;

            private:
               using handle_type = boost::signals2::connection;
               handle_type _handle;

               /**
                * Construct a handle from an internal represenation of a handle
                * In this case a boost::signals2::connection
                *
                * @param _handle - the boost::signals2::connection to wrap
                */
               handle(handle_type&& _handle)
               :_handle(std::move(_handle))
               {}

               friend class method;
         };

         /**
          * Register a provider of this method
          *
          * @tparam T - the type of the provider (functor, lambda)
          * @param provider - the provider
          * @param priority - the priority of this provider, lower is called before higher
          */
         template<typename T>
         handle register_provider(T provider, int priority = 0) {
            return handle(_signal.connect(priority, provider));
         }

         /**
          * inhereted call operator from boost::signals2
          *
          * @throws exception depending on the DispatchPolicy
          */
         template<typename ... Args>
         auto operator()(Args&&... args) -> typename std::enable_if_t<std::is_same<std::tuple<Args...>, args_tuple_type>::value, result_type>
         {
            return _signal(std::forward<Args>(args)...);
         }

      protected:
         method() = default;
         virtual ~method() = default;

         /**
          * Proper deleter for type-erased method
          * note: no type checking is performed at this level
          *
          * @param erased_method_ptr
          */
         static void deleter(void* erased_method_ptr) {
            auto ptr = reinterpret_cast<method*>(erased_method_ptr);
            delete ptr;
         }

         /**
          * get the method* back from an erased pointer
          *
          * @param ptr - the type-erased method pointer
          * @return - the type safe method pointer
          */
         static method* get_method(erased_method_ptr& ptr) {
            return reinterpret_cast<method*>(ptr.get());
         }

         /**
          * Construct a unique_ptr for the type erased method poiner
          * @return
          */
         static erased_method_ptr make_unique() {
            return erased_method_ptr(new method(), &deleter);
         }

         boost::signals2::signal<FunctionSig, DispatchPolicy> _signal;

         friend class appbase::application;
   };


   /**
    *
    * @tparam Tag - API specific discriminator used to distinguish between otherwise identical method signatures
    * @tparam FunctionSig - the signature of the method
    * @tparam DispatchPolicy - dispatch policy that dictates how providers for a method are accessed defaults to @ref first_success_policy
    */
   template< typename Tag, typename FunctionSig, typename DispatchPolicy = first_success_policy<FunctionSig>>
   struct method_decl {
      using method_type = method<FunctionSig, DispatchPolicy>;
      using tag_type = Tag;
   };

   template <typename...Ts>
   std::true_type is_method_decl_impl(const method_decl<Ts...>*);

   std::false_type is_method_decl_impl(...);

   template <typename T>
   using is_method_decl = decltype(is_method_decl_impl(std::declval<T*>()));


}

#pragma pop_macro("N")

