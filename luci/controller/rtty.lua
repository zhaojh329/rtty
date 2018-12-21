module("luci.controller.rtty", package.seeall)

function index()
	entry({"admin", "system", "rtty"}, cbi("rtty"), _("Rtty"), 90)
end
