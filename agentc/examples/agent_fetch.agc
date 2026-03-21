+> net::get;
+> val::tru;
+> io::wr;

#[
  #>"fetches a URL and saves validated content to disk"
  #$(net|io)
  #!(net|io)
  #~(utr)
  #&(net|fs)
  #*(3:500)
]
ƒ fetch_and_save($ url: Str<512>, $ path: Str<256>) -> ()?NetE {
    $ raw: Utr<Str> = net::get(url)?;
    $ clean: Tru<Str> = val::tru(raw)?;
    io::wr(path, clean)?;
    ^(())
}
