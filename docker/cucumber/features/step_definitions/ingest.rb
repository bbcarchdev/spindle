Given(/^a running instance of "([^"]*)" at port (\d+)$/) do |service, port|
  # Try to just open a TCP connection and wait a bit
  http = Net::HTTP.new(service.downcase, port)
  http.read_timeout = 120 # Wait for 120 seconds
  http.start()
  ok = http.started?()
  http.finish()

  # Was it ok ?
  expect(ok).to be(true)
end
