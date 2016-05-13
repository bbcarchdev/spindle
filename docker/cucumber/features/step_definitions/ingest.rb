
When(/^"([^"]*)" is ingested into Twine$/) do |file|
    # POST the to the remote control the file to be ingested
    http = Net::HTTP.new('twine', 8000)
    request = Net::HTTP::Post.new("/ingest")
    request.add_field('Content-Type', 'text/x-nquads')
    request.body = IO.read(file)
    response = http.request(request)
    
    # Print the logs
    a = response.body()
    puts "#{a}"
    
    # Assert if the response was OK
    expect(response).to be_a(Net::HTTPOK)
end

Then(/^"([^"]*)" proxies of type "([^"]*)" should exist in the database$/) do |proxies, type|
  pending # Write code here that turns the phrase above into concrete actions
end