def Linux : StandardSpec<"Linux"> {
  HeaderSpec Errno = HeaderSpec<
      "errno.h",
      [
        Macro<"ENOMEDIUM">,
        Macro<"ENOTBLK">,
        Macro<"EMEDIUMTYPE">,
        Macro<"EBADSLT">,
        Macro<"ECHRNG">,
        Macro<"ERFKILL">,
        Macro<"EUSERS">,
        Macro<"EBADR">,
        Macro<"EL3HLT">,
        Macro<"ENOTUNIQ">,
        Macro<"EXFULL">,
        Macro<"EHOSTDOWN">,
        Macro<"EL3RST">,
        Macro<"ENOPKG">,
        Macro<"ENOCSI">,
        Macro<"EUNATCH">,
        Macro<"EREMCHG">,
        Macro<"ETOOMANYREFS">,
        Macro<"EL2HLT">,
        Macro<"EBADFD">,
        Macro<"EREMOTEIO">,
        Macro<"ENAVAIL">,
        Macro<"ELIBEXEC">,
        Macro<"ESHUTDOWN">,
        Macro<"ENOKEY">,
        Macro<"ESTRPIPE">,
        Macro<"EKEYREJECTED">,
        Macro<"ESRMNT">,
        Macro<"EKEYREVOKED">,
        Macro<"EBADE">,
        Macro<"ELIBBAD">,
        Macro<"EISNAM">,
        Macro<"EBFONT">,
        Macro<"EPFNOSUPPORT">,
        Macro<"EREMOTE">,
        Macro<"EDEADLOCK">,
        Macro<"ENONET">,
        Macro<"EDOTDOT">,
        Macro<"EKEYEXPIRED">,
        Macro<"ELIBSCN">,
        Macro<"ERESTART">,
        Macro<"EBADRQC">,
        Macro<"EUCLEAN">,
        Macro<"ENOANO">,
        Macro<"ELIBACC">,
        Macro<"EHWPOISON">,
        Macro<"ELIBMAX">,
        Macro<"ESOCKTNOSUPPORT">,
        Macro<"ENOTNAM">,
        Macro<"ELNRNG">,
        Macro<"EL2NSYNC">,
        Macro<"EADV">,
        Macro<"ECOMM">,
      ]
  >;

  HeaderSpec SysMMan = HeaderSpec<
      "sys/mman.h",
      [Macro<"MAP_ANONYMOUS">]
  >;

  HeaderSpec SysRandom = HeaderSpec<
      "sys/random.h",
      [
        Macro<"GRND_RANDOM">,
        Macro<"GRND_NONBLOCK">,
        Macro<"GRND_INSECURE">,
      ],
      [], // Types
      [], // Enumerations
      [
        FunctionSpec<
          "getrandom",
          RetValSpec<SSizeTType>,
          [
            ArgSpec<VoidPtr>,
            ArgSpec<SizeTType>,
            ArgSpec<UnsignedIntType>
          ]
        >,
      ]
  >;

  HeaderSpec Signal = HeaderSpec<
      "signal.h",
      [
        Macro<"NSIG">,

        Macro<"SIGHUP">,
        Macro<"SIGINT">,
        Macro<"SIGQUIT">,
        Macro<"SIGILL">,
        Macro<"SIGTRAP">,
        Macro<"SIGABRT">,
        Macro<"SIGIOT">,
        Macro<"SIGBUS">,
        Macro<"SIGFPE">,
        Macro<"SIGKILL">,
        Macro<"SIGUSR1">,
        Macro<"SIGSEGV">,
        Macro<"SIGUSR2">,
        Macro<"SIGPIPE">,
        Macro<"SIGALRM">,
        Macro<"SIGTERM">,
        Macro<"SIGSTKFLT">,
        Macro<"SIGCHLD">,
        Macro<"SIGCONT">,
        Macro<"SIGSTOP">,
        Macro<"SIGTSTP">,
        Macro<"SIGTTIN">,
        Macro<"SIGTTOU">,
        Macro<"SIGURG">,
        Macro<"SIGXCPU">,
        Macro<"SIGXFSZ">,
        Macro<"SIGVTALRM">,
        Macro<"SIGPROF">,
        Macro<"SIGWINCH">,
        Macro<"SIGIO">,
        Macro<"SIGPOLL">,
        Macro<"SIGPWR">,
        Macro<"SIGSYS">,
        Macro<"SIGUNUSED">,
      ]
  >;

  let Headers = [
    Errno,
    SysMMan,
    SysRandom,
    Signal,
  ];
}
