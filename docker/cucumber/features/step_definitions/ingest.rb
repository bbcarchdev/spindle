
When(/^"([^"]*)" is ingested into Twine$/) do |file|
    # POST the to the remote control the file to be ingested
    http = Net::HTTP.new('twine', 8000)
    request = Net::HTTP::Post.new("/ingest")
    request.add_field('Content-Type', 'text/x-nquads')
    request.body = IO.read(file)
    response = http.request(request)

    # Print the logs
    a = response.body()
    #puts "#{a}"

    # Assert if the response was OK
    expect(response).to be_a(Net::HTTPOK)
end

When(/^I count the amount of persons and creative works that are ingested$/) do
        @entities = count("http://quilt/everything.nt?limit=200")
end

When(/^A collection exists for "([^"]*)"$/) do |c_uri|
        uri = URI("http://quilt/")
        params = { :uri => c_uri }
        uri.query = URI.encode_www_form(params)
        response = Net::HTTP.get_response(uri)
        
        expect(response).to be_a(Net::HTTPSeeOther)
        @collection = response['location']
end

Then(/^The number of persons and creative works in the collection should be the same$/) do
        n = count("http://quilt#{@collection}.nt?limit=200")

        expect(n).to eq(@entities)
end
