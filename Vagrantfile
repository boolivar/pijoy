Vagrant.configure("2") do |config|
  
  config.vm.box = "bento/ubuntu-16.04"

  config.vm.provision "docker" do |d|
    d.build_image "/vagrant", args: "-t rpi-build"
  end
end
