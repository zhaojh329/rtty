local m, s, o

m = Map("rtty", translate("Rtty"))
m:chain("luci")


s = m:section(TypedSection, "rtty")
s.anonymous = true
s.addremove = true

o = s:option(Value, "id", translate("ID"))

o = s:option(Value, "host", translate("Server"))

o = s:option(Value, "port", translate("Port"))
o.datatype = "port"


o = s:option(Flag, "ssl", translate("SSL"))


o = s:option(Value, "description", translate("Description"))


o = s:option(Value, "token", translate("Token"))

return m
